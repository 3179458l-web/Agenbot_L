"""
calibrate.py —— 交互式标定工具（只需运行一次）

使用方法：
  python calibrate.py

步骤：
  1. 在机械臂工作台上放 4 个标记点（推荐用彩色胶带贴十字）
  2. 用机械臂末端接触每个标记点，记录机械臂坐标 (X,Y)（单位 mm）
  3. 运行此脚本，画面出现后，用鼠标点击每个标记点对应的像素位置
  4. 输入各点的机械臂坐标
  5. 标定结果自动保存到 config/camera_params.json

标记点建议布局（从上方看）：
  P1 ─────── P2
  │     机     │
  │     械     │
  │     臂     │
  P4 ─────── P3
  四个角覆盖尽量大的工作区域，精度更高
"""

import cv2
import numpy as np
from camera import Camera
from calibration import Calibration

NUM_POINTS = 4  # 标定点数量


def run_calibration():
    pixel_pts = []
    frame_ref = None

    def on_mouse(event, x, y, flags, param):
        nonlocal pixel_pts, frame_ref
        if event == cv2.EVENT_LBUTTONDOWN:
            if len(pixel_pts) < NUM_POINTS:
                pixel_pts.append((x, y))
                idx = len(pixel_pts)
                print(f"  已选 P{idx}: 像素 ({x}, {y})")
                # 在画面上标记
                cv2.circle(frame_ref, (x, y), 8, (0, 255, 0), -1)
                cv2.putText(frame_ref, f"P{idx}", (x + 10, y - 5),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
                cv2.imshow("Calibration", frame_ref)

    print("=" * 50)
    print("  视觉标定工具")
    print("=" * 50)
    print(f"请在工作台上放置 {NUM_POINTS} 个标记点（建议四角分布）")
    print("画面出现后，按顺序点击每个标记点的像素位置")
    print()

    with Camera() as cam:
        cv2.namedWindow("Calibration")
        cv2.setMouseCallback("Calibration", on_mouse)

        print("按 空格 冻结画面并开始点击标记点，按 r 重新采集，按 q 退出")
        frozen = False

        while True:
            if not frozen:
                frame_ref = cam.grab_frame()
                # 提示文字
                cv2.putText(frame_ref, "按 空格 冻结画面开始标定",
                            (20, 40), cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 0), 2)
                cv2.imshow("Calibration", frame_ref)

            key = cv2.waitKey(30) & 0xFF
            if key == ord('q'):
                print("退出标定")
                cv2.destroyAllWindows()
                return
            elif key == ord(' ') and not frozen:
                frozen = True
                pixel_pts = []
                print(f"\n画面已冻结，请依次点击 {NUM_POINTS} 个标记点")
            elif key == ord('r'):
                frozen = False
                pixel_pts = []
                print("重新采集...")

            if len(pixel_pts) == NUM_POINTS:
                cv2.waitKey(500)
                cv2.destroyAllWindows()
                break

    # 输入机械臂世界坐标
    print(f"\n已选择 {NUM_POINTS} 个像素点，现在输入对应的机械臂坐标（单位 mm）")
    print("（用机械臂末端接触标记点，从控读取 joint 角度后通过正运动学算出 XY）\n")

    world_pts = []
    for i, (pu, pv) in enumerate(pixel_pts, 1):
        print(f"P{i} 像素: ({pu}, {pv})")
        while True:
            try:
                x = float(input(f"  P{i} 机械臂 X (mm): "))
                y = float(input(f"  P{i} 机械臂 Y (mm): "))
                world_pts.append((x, y))
                break
            except ValueError:
                print("  请输入数字")

    z_mm_str = input("\n桌面相对机械臂基座的 Z 高度 (mm，向下为负，直接回车默认 0): ").strip()
    z_mm = float(z_mm_str) if z_mm_str else 0.0

    # 计算并保存
    calib = Calibration.from_point_pairs(pixel_pts, world_pts, z_mm)
    calib.save()

    # 验证
    print("\n--- 标定验证 ---")
    for i, (pu, pv) in enumerate(pixel_pts, 1):
        wx, wy, wz = calib.pixel_to_world(pu, pv)
        ex, ey = world_pts[i - 1]
        err = ((wx - ex) ** 2 + (wy - ey) ** 2) ** 0.5
        print(f"P{i}: 预测({wx:.1f}, {wy:.1f}) 实际({ex:.1f}, {ey:.1f}) 误差={err:.2f}mm")

    print("\n标定完成！结果已保存到 config/camera_params.json")


if __name__ == "__main__":
    run_calibration()
