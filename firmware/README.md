# firmware 固件目录

本目录用于存放所有 Arduino / ESP32 固件。

## 当前目录规划

```text
firmware/
├─ tire_temp_height_node_v2/                  # 历史参考
├─ tire_temp_height_node_v3/                  # 当前 RaceChrono BLE only 主线
├─ brake_temp_12v_max31856/                   # 独立热电偶卡钳测温版本
├─ brake_temp_12v_rs485/                      # 初代RS485红外刹车盘版本，不带显示报警，非重点
├─ brake_temp_alarm_display_12v_rs485/        # 当前刹车节点板重点版本
└─ central_display/
```

## 重要版本边界

刹车节点目前一共有三个版本：

```text
1. brake_temp_12v_max31856
   MAX31856 + K 型热电偶，测卡钳，当前未接入报警显示。

2. brake_temp_12v_rs485
   RS485 红外测温传感器，测刹车盘，不带显示报警，是初代版本，非重点。

3. brake_temp_alarm_display_12v_rs485
   RS485 红外测温传感器，测刹车盘，带显示、ALM、按键、蜂鸣器、RaceChrono，是当前重点。
```

当前没有 `firmware/brake_temp_12v_rs485_racechrono_node/` 这个实际版本，不要让 Codex 基于这个名字继续开发。

## 放置规则

独立研发目录 `wired_tire_channel` 用于新有线胎温中控的 C3 采集/BLE 单元，共享库位于 `libraries/WiredTireCore`。当前默认离线构建、禁用实际 I/O，不能作为旧 v3 的替换固件；完整测试和接口见 `docs/11_四路有线胎温中控接口.md`。

`wired_tire_display` 为配套 P4 显示固件，目标 7 英寸 1024×600；通过独立编译脚本选择 P4，不能按旧固件 C3 目标批量编译。离线软件包与 Arduino IDE 安装说明见 `docs/12_四路有线胎温离线软件交付说明.md`。

- 每个固件目录至少包含一个 `.ino` 文件和一个 `README.md`。
- 稳定版本不要直接覆盖，建议保留版本号或使用 Git tag。
- Codex 修改代码前必须阅读仓库根目录 `CODEX.md` 和 `AGENTS.md`。
- 修改 `.ino` 时，用户偏好是输出完整终版代码，而不是零散 patch。
