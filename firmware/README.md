# firmware 固件目录

本目录用于存放所有 Arduino / ESP32 固件。

## 目录规划

```text
firmware/
├─ tire_temp_height_node_v2/
├─ tire_temp_height_node_v3/
├─ brake_temp_12v_max31856/
├─ brake_temp_12v_rs485_racechrono_node/
├─ brake_temp_alarm_display_12v_rs485/
└─ central_display/
```

## 放置规则

- 每个固件目录至少包含一个 `.ino` 文件和一个 `README.md`。
- 稳定版本不要直接覆盖，建议保留版本号或使用 Git tag。
- Codex 修改代码前必须阅读仓库根目录 `CODEX.md` 和 `AGENTS.md`。
- 修改 `.ino` 时，用户偏好是输出完整终版代码，而不是零散 patch。
