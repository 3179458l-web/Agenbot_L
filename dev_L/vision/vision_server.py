"""
vision_server.py —— 视觉模块对外接口

提供两种调用方式：
  1. Python 直接 import（供 agent 层调用）
  2. TCP Socket 服务（供 ESP32 主控通过 WiFi 调用）

TCP 协议（JSON over TCP，按行分隔）：
  请求: {"action": "detect", "query": "找红色方块"}\\n
  响应: {"ok": true, "objects": [...]}\\n
  响应: {"ok": false, "error": "..."}\\n

objects 格式：
  [{"label":"红色方块", "bbox":[x1,y1,x2,y2], "center":[cx,cy],
    "world":[X_mm, Y_mm, Z_mm], "confidence":"high"}, ...]
"""

import json
import socket
import threading
import os

import cv2

from camera import Camera
from detector import detect_objects, draw_detections
from calibration import Calibration


class VisionServer:
    def __init__(self, host: str = "0.0.0.0", port: int = 5555):
        self._host = host
        self._port = port
        self._cam = Camera()

        # 标定文件存在则加载，否则仅返回像素坐标
        try:
            self._calib = Calibration.load()
        except FileNotFoundError:
            print("[VisionServer] 警告：未找到标定文件，world 坐标将为 null")
            self._calib = None

    # ------------------------------------------------------------------
    # 核心方法（可直接被 Python 代码调用）
    # ------------------------------------------------------------------

    def detect(self, query: str = "") -> list[dict]:
        """
        抓取一帧并检测目标，返回带世界坐标的结果列表。

        :param query: 可选的用户意图，如 "找最近的红色物体"
        :return: list of dicts，含 label / bbox / center / world / confidence
        """
        frame = self._cam.grab_frame()
        detections = detect_objects(frame, query)

        # 补充世界坐标
        for det in detections:
            if self._calib:
                cx, cy = det["center"]
                x, y, z = self._calib.pixel_to_world(cx, cy)
                det["world"] = [round(x, 1), round(y, 1), round(z, 1)]
            else:
                det["world"] = None

        return detections

    def detect_and_show(self, query: str = "") -> list[dict]:
        """检测并弹出可视化窗口（调试用）"""
        frame = self._cam.grab_frame()
        detections = detect_objects(frame, query)
        for det in detections:
            if self._calib:
                cx, cy = det["center"]
                x, y, z = self._calib.pixel_to_world(cx, cy)
                det["world"] = [round(x, 1), round(y, 1), round(z, 1)]
            else:
                det["world"] = None

        vis = draw_detections(frame, detections)

        # 在画面上叠加世界坐标
        for det in detections:
            if det["world"]:
                cx, cy = det["center"]
                wx, wy, wz = det["world"]
                cv2.putText(vis, f"({wx},{wy}mm)", (cx - 30, cy + 20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1)

        cv2.imshow("Vision Detection", vis)
        cv2.waitKey(1)
        return detections

    # ------------------------------------------------------------------
    # TCP Socket 服务（供 ESP32 主控调用）
    # ------------------------------------------------------------------

    def start_tcp_server(self):
        """启动 TCP 服务，阻塞运行。建议在独立线程中调用。"""
        srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        srv.bind((self._host, self._port))
        srv.listen(5)
        print(f"[VisionServer] TCP 服务启动：{self._host}:{self._port}")

        while True:
            conn, addr = srv.accept()
            print(f"[VisionServer] 连接来自 {addr}")
            threading.Thread(target=self._handle_client,
                             args=(conn,), daemon=True).start()

    def _handle_client(self, conn: socket.socket):
        try:
            buf = b""
            while True:
                data = conn.recv(1024)
                if not data:
                    break
                buf += data
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    resp = self._process_request(line.decode().strip())
                    conn.sendall((json.dumps(resp, ensure_ascii=False) + "\n").encode())
        except Exception as e:
            print(f"[VisionServer] 连接异常: {e}")
        finally:
            conn.close()

    def _process_request(self, raw: str) -> dict:
        try:
            req = json.loads(raw)
        except json.JSONDecodeError:
            return {"ok": False, "error": "invalid json"}

        action = req.get("action", "")
        if action == "detect":
            try:
                objects = self.detect(req.get("query", ""))
                return {"ok": True, "objects": objects}
            except Exception as e:
                return {"ok": False, "error": str(e)}
        elif action == "ping":
            return {"ok": True, "message": "pong"}
        else:
            return {"ok": False, "error": f"unknown action: {action}"}

    def release(self):
        self._cam.release()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.release()


# ------------------------------------------------------------------
# 直接运行：启动 TCP 服务 + 本地调试预览
# ------------------------------------------------------------------

if __name__ == "__main__":
    if not os.environ.get("ANTHROPIC_API_KEY"):
        print("请先设置环境变量 ANTHROPIC_API_KEY")
        exit(1)

    with VisionServer() as server:
        # TCP 服务在后台线程运行
        t = threading.Thread(target=server.start_tcp_server, daemon=True)
        t.start()

        print("视觉服务已启动，按 空格 触发检测，按 q 退出")
        import sys
        query = sys.argv[1] if len(sys.argv) > 1 else ""

        while True:
            key = cv2.waitKey(30) & 0xFF
            if key == ord('q'):
                break
            elif key == ord(' '):
                results = server.detect_and_show(query)
                print(json.dumps(results, ensure_ascii=False, indent=2))

        cv2.destroyAllWindows()
