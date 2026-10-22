#!/usr/bin/env python3
from pathlib import Path
import sys


REPO_ROOT = Path(__file__).resolve().parents[1]


CHECKS = [
    ("docs/11_四路有线胎温中控接口.md", ["硬件关卡未通过", "不是 DIY CAN", "SPI 快照 v1"]),
    ("docs/12_四路有线胎温离线软件交付说明.md", ["离线研发版本", "不是 DIY CAN", "hardware_validated=false"]),
    ("firmware/wired_tire_channel/README.md", ["默认构建 WIRED_TIRE_OFFLINE_BUILD=1", "不是旧无线 v3 替换固件"]),
    ("firmware/wired_tire_display/README.md", ["默认离线构建", "尚未验证"]),
    ("hardware/wired_tire_central/README.md", ["设计验证阶段", "未提供制造文件"]),
    (
        "README.md",
        [
            "胎温高度节点 v3 RaceChrono BLE only",
            "不再向中控发送胎温数据",
            "brake_temp_alarm_display_12V_RS485",
            "BLE DIY CAN 自定义通道",
            "RejsaRubberTrac 风格 BLE 热图不是 DIY CAN",
        ],
    ),
    (
        "CODEX.md",
        [
            "胎温高度节点板 v3 RaceChrono BLE only",
            "不要把胎温高度 v3 的 RejsaRubberTrac BLE 热图写成 DIY CAN PID",
            "brake_temp_12v_max31856",
            "测卡钳温度",
            "当前未接入报警显示",
            "brake_temp_alarm_display_12V_RS485",
        ],
    ),
    (
        "AGENTS.md",
        [
            "RaceChrono Pro 是本项目的核心使用场景",
            "胎温高度 v3 的 RejsaRubberTrac 风格 BLE 热图不是 DIY CAN",
        ],
    ),
    (
        "docs/04_RaceChrono_BLE说明.md",
        [
            "胎温高度节点 v3 的胎温热图走 RejsaRubberTrac 风格 BLE，不是 DIY CAN BLE",
            "只有 DIY CAN BLE 自定义通道相关改动，才要求列出 PID / CAN ID 和 RaceChrono 计算公式",
        ],
    ),
    (
        "docs/08_12V_RS485刹车节点说明.md",
        [
            "当前 BLE DIY CAN 自定义通道 PID 与 RaceChrono 公式",
            "0x100 Brake FL",
            "0x107 Brake Alarm",
            "该规则只针对 DIY CAN 自定义通道，不适用于胎温高度 v3 的 RejsaRubberTrac 风格 BLE 热图",
        ],
    ),
    (
        "docs/09_胎温高度v3_RaceChrono_BLE_ONLY说明.md",
        [
            "BLE only. No WiFi, no ESP-NOW, no central display link.",
            "RejsaRubberTrac 风格 BLE",
        ],
    ),
    (
        "docs/10_文档同步规则.md",
        [
            "胎温高度 v3 的 RejsaRubberTrac 风格 BLE 热图不适用上述 PID / 公式要求",
            "brake_temp_alarm_display_12v_rs485",
            "带显示、ALM、按键、蜂鸣器、RaceChrono BLE",
        ],
    ),
    (
        "firmware/brake_temp_alarm_display_12v_rs485/README.md",
        [
            "RaceChrono DIY CAN 自定义通道 PIDs",
            "RaceChrono formula",
            "不适用于胎温高度 v3 的 RejsaRubberTrac 风格 BLE 热图",
        ],
    ),
]


FORBIDDEN = [
    (
        "docs/09_胎温高度v3_RaceChrono_BLE_ONLY说明.md",
        [
            "ESP-NOW -> 中控发送胎温",
        ],
        "The v3 tire node doc may mention old removed links only in an explicit 'not used' context; review manually if this fails.",
    ),
]


def read_text(path: Path) -> str:
    return path.read_text(encoding="utf-8")


def main() -> int:
    failures = []

    for rel_path, required_terms in CHECKS:
        path = REPO_ROOT / rel_path
        if not path.exists():
            failures.append(f"{rel_path}: file missing")
            continue

        text = read_text(path)
        for term in required_terms:
            if term not in text:
                failures.append(f"{rel_path}: missing required text -> {term}")

    for rel_path, terms, note in FORBIDDEN:
        path = REPO_ROOT / rel_path
        if not path.exists():
            continue
        text = read_text(path)
        for term in terms:
            if term in text and "不再使用" not in text and "不加入" not in text:
                failures.append(f"{rel_path}: possible forbidden stale text -> {term}. {note}")

    if failures:
        print("Documentation consistency check failed:")
        for item in failures:
            print(f"- {item}")
        return 1

    print("Documentation consistency check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
