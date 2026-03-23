"""
tools.py —— Agent 可调用的工具定义与执行

每个工具：
  1. SCHEMA   : 传给 Claude 的 JSON Schema（告诉 LLM 怎么调用）
  2. executor : 实际执行函数，返回 dict（结果会序列化为 JSON 喂回 LLM）

工具列表：
  arm_move_cartesian  移动末端到 XYZ (mm)
  arm_move_joint      单关节角度控制
  arm_grab            夹爪抓取
  arm_release         夹爪释放
  arm_home            回零姿态
  vision_detect       视觉识别，返回桌面物体及坐标
"""

import sys
import os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'vision'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', '..', 'scripts'))

from typing import Any

# ---- 懒加载硬件接口（无硬件时降级为离线模式）----
_kinematics = None
_controller = None
_vision = None


def _get_kinematics():
    global _kinematics, _controller
    if _kinematics is None:
        try:
            from arm_controller import ArmController
            from arm_kinematics import ArmKinematics
            _controller = ArmController()
            _kinematics = ArmKinematics(
                os.path.join(os.path.dirname(__file__), '..', '..', 'config', 'arm_config.json'),
                controller=_controller,
            )
        except Exception as e:
            return None, f"硬件未连接: {e}"
    return _kinematics, None


def _get_vision():
    global _vision
    if _vision is None:
        try:
            from vision_server import VisionServer
            _vision = VisionServer()
        except Exception as e:
            return None, f"视觉模块未就绪: {e}"
    return _vision, None


# ======================================================================
# 工具 Schema（Claude tool_use 格式）
# ======================================================================

TOOL_SCHEMAS = [
    {
        "name": "arm_move_cartesian",
        "description": (
            "将机械臂末端移动到笛卡尔坐标位置（毫米）。"
            "原点为机械臂基座中心，X 为正前方，Y 为左方，Z 为上方。"
            "典型工作范围：X 50-250mm，Y ±150mm，Z -50-200mm。"
        ),
        "input_schema": {
            "type": "object",
            "properties": {
                "x": {"type": "number", "description": "前后方向，mm，正前"},
                "y": {"type": "number", "description": "左右方向，mm，正左"},
                "z": {"type": "number", "description": "上下方向，mm，正上"},
                "pitch": {"type": "number", "description": "末端俯仰角度（度），0=水平，-90=向下，默认0"},
                "speed": {"type": "integer", "description": "速度 1-100，默认 50"},
            },
            "required": ["x", "y", "z"],
        },
    },
    {
        "name": "arm_move_joint",
        "description": "直接控制机械臂单个关节转到指定角度。逻辑关节编号：0=底座旋转，1=肩部，2=肘部，3=腕俯仰，4=腕旋转，5=夹爪。",
        "input_schema": {
            "type": "object",
            "properties": {
                "joint": {"type": "integer", "description": "逻辑关节编号 0-5"},
                "angle": {"type": "number", "description": "目标角度（度）"},
                "speed": {"type": "integer", "description": "速度 1-100，默认 50"},
            },
            "required": ["joint", "angle"],
        },
    },
    {
        "name": "arm_grab",
        "description": "夹爪抓取。先移动到目标位置后再调用此工具。",
        "input_schema": {
            "type": "object",
            "properties": {
                "force": {"type": "integer", "description": "夹取力度 0-100，默认 60。较小物体用较小力度。"},
            },
            "required": [],
        },
    },
    {
        "name": "arm_release",
        "description": "夹爪完全释放，放开当前抓取的物体。",
        "input_schema": {"type": "object", "properties": {}},
    },
    {
        "name": "arm_home",
        "description": "机械臂回到 home 姿态（所有关节归 90°），用于复位或避免碰撞。",
        "input_schema": {"type": "object", "properties": {}},
    },
    {
        "name": "vision_detect",
        "description": (
            "调用摄像头识别桌面上的物体，返回物体列表及其位置坐标。"
            "在抓取任何物体前应先调用此工具确认目标位置。"
        ),
        "input_schema": {
            "type": "object",
            "properties": {
                "query": {
                    "type": "string",
                    "description": "描述要找的目标，如'红色方块'、'最近的物体'，留空则返回所有物体",
                },
            },
            "required": [],
        },
    },
]


# ======================================================================
# 工具执行器
# ======================================================================

def execute_tool(tool_name: str, args: dict) -> dict:
    """
    统一执行入口。
    返回 dict，会被序列化为 JSON 喂回 LLM。
    """
    executors = {
        "arm_move_cartesian": _exec_move_cartesian,
        "arm_move_joint":     _exec_move_joint,
        "arm_grab":           _exec_grab,
        "arm_release":        _exec_release,
        "arm_home":           _exec_home,
        "vision_detect":      _exec_vision_detect,
    }
    fn = executors.get(tool_name)
    if fn is None:
        return {"ok": False, "error": f"未知工具: {tool_name}"}
    try:
        return fn(args)
    except Exception as e:
        return {"ok": False, "error": str(e)}


def _exec_move_cartesian(args: dict) -> dict:
    km, err = _get_kinematics()
    if err:
        return {"ok": False, "error": err}
    x = args["x"]
    y = args["y"]
    z = args["z"]
    pitch = args.get("pitch", 0.0)
    speed = args.get("speed", 50)
    result = km.move_to(x, y, z, pitch=pitch, speed=speed)
    if result["success"]:
        pos = result["position"]
        return {
            "ok": True,
            "position": {"x": round(pos[0], 1), "y": round(pos[1], 1), "z": round(pos[2], 1)},
            "joint_angles": [round(a, 1) for a in result["joint_angles"]],
        }
    return {"ok": False, "error": result["error"]}


def _exec_move_joint(args: dict) -> dict:
    km, err = _get_kinematics()
    if err:
        return {"ok": False, "error": err}
    joint = int(args["joint"])
    angle = float(args["angle"])
    speed = int(args.get("speed", 50))
    # 通过 kinematics 的 controller 发送
    km.controller.move_joint(km.logical_to_physical(joint), int(round(angle)), speed)
    return {"ok": True, "joint": joint, "angle": angle}


def _exec_grab(args: dict) -> dict:
    km, err = _get_kinematics()
    if err:
        return {"ok": False, "error": err}
    force = int(args.get("force", 60))
    # 夹爪关闭：force 映射到 gripper 最小角度
    bounds = km.get_logical_joint_bounds()[5]
    angle = bounds["min"] + (bounds["max"] - bounds["min"]) * (1 - force / 100.0)
    km.set_gripper(int(round(angle)))
    return {"ok": True, "force": force, "gripper_angle": round(angle, 1)}


def _exec_release(args: dict) -> dict:
    km, err = _get_kinematics()
    if err:
        return {"ok": False, "error": err}
    km.open_gripper()
    return {"ok": True}


def _exec_home(args: dict) -> dict:
    km, err = _get_kinematics()
    if err:
        return {"ok": False, "error": err}
    km.move_to_home()
    return {"ok": True, "message": "已回到 home 姿态"}


def _exec_vision_detect(args: dict) -> dict:
    vs, err = _get_vision()
    if err:
        return {"ok": False, "error": err}
    query = args.get("query", "")
    objects = vs.detect(query)
    return {
        "ok": True,
        "count": len(objects),
        "objects": objects,
    }
