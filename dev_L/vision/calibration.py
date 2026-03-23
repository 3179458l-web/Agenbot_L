"""
calibration.py —— 相机标定 & 像素→世界坐标变换

原理：
  使用单应性矩阵（Homography）将像素坐标映射到机械臂工作台平面坐标（mm）。
  这是一个平面标定方案（假设所有目标物体在同一高度的桌面上），
  不需要深度相机，只需在桌面上放 4 个已知坐标的标记点做一次标定即可。

标定流程（由 calibrate.py 完成）：
  1. 在桌面上放 4 个标记点（如 ArUco 码或彩色小球），量出它们的机械臂坐标 (X,Y)
  2. 拍一帧，记录 4 点的像素坐标 (u,v)
  3. cv2.findHomography() 计算单应矩阵 H，保存到 config/camera_params.json

使用：
  calib = Calibration.load()
  x_mm, y_mm = calib.pixel_to_world(cx, cy)
  # Z 轴：固定桌面高度（由配置文件指定）
"""

import json
import numpy as np
import cv2
from pathlib import Path

CONFIG_PATH = Path(__file__).parent / "config" / "camera_params.json"


class Calibration:
    def __init__(self, H: np.ndarray, z_mm: float = 0.0):
        """
        :param H:    3x3 单应矩阵（像素 → 机械臂平面坐标，单位 mm）
        :param z_mm: 桌面相对于机械臂基座的 Z 高度（mm），负数表示桌面低于基座
        """
        self.H = H
        self.z_mm = z_mm

    def pixel_to_world(self, u: float, v: float) -> tuple[float, float, float]:
        """
        将像素坐标 (u,v) 转换为机械臂世界坐标 (X,Y,Z)，单位 mm。

        :return: (x_mm, y_mm, z_mm)
        """
        pt = np.array([[[u, v]]], dtype=np.float32)
        world = cv2.perspectiveTransform(pt, self.H)
        x_mm = float(world[0][0][0])
        y_mm = float(world[0][0][1])
        return x_mm, y_mm, self.z_mm

    def save(self, path: Path = CONFIG_PATH):
        """保存标定结果到 JSON"""
        path.parent.mkdir(parents=True, exist_ok=True)
        data = {
            "homography": self.H.tolist(),
            "z_mm": self.z_mm,
        }
        path.write_text(json.dumps(data, indent=2))
        print(f"[Calibration] 已保存到 {path}")

    @classmethod
    def load(cls, path: Path = CONFIG_PATH) -> "Calibration":
        """从 JSON 加载标定结果"""
        if not path.exists():
            raise FileNotFoundError(
                f"标定文件不存在：{path}\n请先运行 calibrate.py 完成标定。"
            )
        data = json.loads(path.read_text())
        H = np.array(data["homography"], dtype=np.float64)
        z_mm = data.get("z_mm", 0.0)
        print(f"[Calibration] 已加载，Z={z_mm}mm")
        return cls(H, z_mm)

    @classmethod
    def from_point_pairs(
        cls,
        pixel_pts: list[tuple[float, float]],
        world_pts: list[tuple[float, float]],
        z_mm: float = 0.0,
    ) -> "Calibration":
        """
        用标定点对计算单应矩阵。

        :param pixel_pts: 像素坐标列表 [(u1,v1), (u2,v2), ...]，至少 4 点
        :param world_pts: 对应的机械臂坐标 [(x1,y1), (x2,y2), ...]，单位 mm
        :param z_mm:      桌面 Z 高度
        """
        if len(pixel_pts) < 4 or len(world_pts) < 4:
            raise ValueError("至少需要 4 个标定点")

        src = np.array(pixel_pts, dtype=np.float32)
        dst = np.array(world_pts, dtype=np.float32)
        H, mask = cv2.findHomography(src, dst, cv2.RANSAC, 5.0)

        if H is None:
            raise RuntimeError("单应矩阵计算失败，请检查标定点是否共线")

        inliers = int(mask.sum()) if mask is not None else len(pixel_pts)
        print(f"[Calibration] 单应矩阵计算完成，内点 {inliers}/{len(pixel_pts)}")
        return cls(H, z_mm)
