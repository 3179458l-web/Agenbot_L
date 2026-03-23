"""
camera.py —— USB 摄像头采集封装

职责：
  - 打开 USB UVC 摄像头（BL-1080p-S10）
  - 提供 grab_frame() 返回 BGR numpy array
  - 提供 preview() 用于调试预览
"""

import cv2
import numpy as np


class Camera:
    def __init__(self, device_index: int = 1, width: int = 1280, height: int = 720):
        """
        :param device_index: 摄像头设备号，默认 0（第一个 USB 摄像头）
        :param width/height: 采集分辨率，720p 够用且推理更快
        """
        self._cap = cv2.VideoCapture(device_index, cv2.CAP_DSHOW)  # DSHOW 在 Windows 下延迟更低
        if not self._cap.isOpened():
            raise RuntimeError(f"无法打开摄像头 index={device_index}")

        self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        self._cap.set(cv2.CAP_PROP_AUTOFOCUS, 1)

        # 预热：丢弃前几帧（曝光自适应需要时间）
        for _ in range(5):
            self._cap.read()

        actual_w = int(self._cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        print(f"[Camera] 已打开，实际分辨率 {actual_w}x{actual_h}")

    def grab_frame(self) -> np.ndarray:
        """
        抓取一帧，返回 BGR ndarray (H, W, 3)。
        失败时抛出 RuntimeError。
        """
        ok, frame = self._cap.read()
        if not ok or frame is None:
            raise RuntimeError("摄像头读取失败")
        return frame

    def grab_frame_rgb(self) -> np.ndarray:
        """返回 RGB 格式（用于 LLM API 发送）"""
        return cv2.cvtColor(self.grab_frame(), cv2.COLOR_BGR2RGB)

    def preview(self):
        """
        实时预览窗口，按 q 或 ESC 退出。
        用于调试摄像头是否正常工作。
        """
        print("[Camera] 预览模式，按 q 退出")
        while True:
            frame = self.grab_frame()
            cv2.imshow("Camera Preview", frame)
            key = cv2.waitKey(1) & 0xFF
            if key in (ord('q'), 27):
                break
        cv2.destroyAllWindows()

    def release(self):
        self._cap.release()

    def __enter__(self):
        return self

    def __exit__(self, *_):
        self.release()


if __name__ == "__main__":
    # 直接运行此文件 = 预览测试
    with Camera() as cam:
        cam.preview()
