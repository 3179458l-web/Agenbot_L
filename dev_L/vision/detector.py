"""
detector.py —— 视觉目标检测（Vision LLM + OpenCV 辅助）

流程：
  1. 把当前帧编码为 base64 JPEG
  2. 调用 Claude claude-sonnet-4-6 Vision API，描述场景、返回目标 bbox
  3. 解析响应，得到 {object, bbox:[u1,v1,u2,v2], center:[cx,cy]}

LLM 返回格式（JSON）：
  {
    "objects": [
      {"label": "红色方块", "bbox": [120, 80, 220, 180], "confidence": "high"},
      {"label": "蓝色圆柱", "bbox": [300, 150, 380, 260], "confidence": "medium"}
    ]
  }
  bbox 单位：像素，[x_min, y_min, x_max, y_max]
"""

import base64
import json
import re
import cv2
import numpy as np
import anthropic

from camera import Camera


# Claude API 客户端（从环境变量 ANTHROPIC_API_KEY 读取 key）
_client = anthropic.Anthropic()

# 系统提示：固定场景（机械臂抓取工作台）
_SYSTEM_PROMPT = """你是一个机械臂视觉感知模块。
图像来自固定在工作台上方的摄像头，画面中是一个机械臂工作区域。

你的任务：
1. 识别画面中桌面上所有可抓取的物品
2. 对每个物品，给出：
   - label: 简短描述（颜色 + 形状/类别，如"红色方块"、"蓝色水瓶"）
   - bbox: 像素坐标 [x_min, y_min, x_max, y_max]（图像左上角为原点）
   - confidence: "high" / "medium" / "low"

只返回 JSON，不要任何解释文字，格式：
{"objects": [{"label": "...", "bbox": [x1,y1,x2,y2], "confidence": "high"}, ...]}

如果画面中没有可抓取物品，返回：{"objects": []}
"""


def _encode_frame(frame: np.ndarray, quality: int = 85) -> str:
    """将 BGR 帧编码为 base64 JPEG 字符串"""
    _, buf = cv2.imencode(".jpg", frame, [cv2.IMWRITE_JPEG_QUALITY, quality])
    return base64.standard_b64encode(buf.tobytes()).decode("utf-8")


def detect_objects(frame: np.ndarray, query: str = "") -> list[dict]:
    """
    调用 Vision LLM 检测画面中的物体。

    :param frame:  BGR 帧（来自 camera.grab_frame()）
    :param query:  可选的用户指令，如 "找到红色的物体"，留空则识别所有物体
    :return: list of {"label", "bbox":[x1,y1,x2,y2], "center":[cx,cy], "confidence"}
    """
    b64 = _encode_frame(frame)

    user_content = [
        {
            "type": "image",
            "source": {
                "type": "base64",
                "media_type": "image/jpeg",
                "data": b64,
            },
        }
    ]
    if query:
        user_content.append({"type": "text", "text": f"用户指令：{query}"})
    else:
        user_content.append({"type": "text", "text": "识别画面中所有可抓取物品。"})

    resp = _client.messages.create(
        model="claude-sonnet-4-6",
        max_tokens=1024,
        system=_SYSTEM_PROMPT,
        messages=[{"role": "user", "content": user_content}],
    )

    raw = resp.content[0].text.strip()

    # 从响应中提取 JSON（防止 LLM 多输出了文字）
    m = re.search(r'\{.*\}', raw, re.DOTALL)
    if not m:
        return []

    try:
        data = json.loads(m.group())
    except json.JSONDecodeError:
        return []

    results = []
    for obj in data.get("objects", []):
        bbox = obj.get("bbox", [])
        if len(bbox) != 4:
            continue
        x1, y1, x2, y2 = bbox
        cx = (x1 + x2) // 2
        cy = (y1 + y2) // 2
        results.append({
            "label": obj.get("label", "unknown"),
            "bbox": [x1, y1, x2, y2],
            "center": [cx, cy],
            "confidence": obj.get("confidence", "medium"),
        })

    return results


def draw_detections(frame: np.ndarray, detections: list[dict]) -> np.ndarray:
    """
    在帧上绘制检测结果（bbox + label），用于调试可视化。
    返回带标注的新帧（不修改原帧）。
    """
    out = frame.copy()
    for det in detections:
        x1, y1, x2, y2 = det["bbox"]
        cx, cy = det["center"]
        label = det["label"]
        conf = det["confidence"]

        color = {"high": (0, 255, 0), "medium": (0, 200, 255), "low": (0, 0, 255)}.get(conf, (128, 128, 128))

        cv2.rectangle(out, (x1, y1), (x2, y2), color, 2)
        cv2.circle(out, (cx, cy), 5, color, -1)
        cv2.putText(out, label, (x1, y1 - 8),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, color, 2)

    return out


if __name__ == "__main__":
    """直接运行 = 实时检测预览（每按空格触发一次 LLM 检测）"""
    import os
    if not os.environ.get("ANTHROPIC_API_KEY"):
        print("请先设置环境变量 ANTHROPIC_API_KEY")
        exit(1)

    import threading

    with Camera() as cam:
        print("按 回车 触发检测，输入 q 回车 退出")
        detections = []
        trigger = threading.Event()
        stop = threading.Event()

        def input_thread():
            while not stop.is_set():
                cmd = input()
                if cmd.strip().lower() == 'q':
                    stop.set()
                else:
                    trigger.set()

        t = threading.Thread(target=input_thread, daemon=True)
        t.start()

        while not stop.is_set():
            frame = cam.grab_frame()
            vis = draw_detections(frame, detections)
            cv2.imshow("Detector", vis)
            cv2.waitKey(30)

            if trigger.is_set():
                trigger.clear()
                print("检测中...")
                detections = detect_objects(frame)
                print(f"结果: {json.dumps(detections, ensure_ascii=False, indent=2)}")

        cv2.destroyAllWindows()
