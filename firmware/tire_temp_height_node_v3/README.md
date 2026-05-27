# 胎温高度节点 v3 固件

## 用途

用于胎温高度节点 v3 量产候选硬件：

- ESP32-C3
- MLX90640
- VL53L1X
- LTC2954-2 单按键电源控制
- RaceChrono BLE
- ESP-NOW / 中控显示扩展

## 状态

```text
当前状态：待上传稳定固件
```

## 关键硬件结论

- U_PWR 应使用 `LTC2954CTS8-2#TRPBF`
- `R_BTN_PULL` 应 DNP / 删除
- `PWR_KILL -> SYS_3V3` 需要 47kΩ 上拉
- 不要随意修改 `PWR_KILL`、`PWR_INT`、按键和电源保持逻辑

## 上传固件后需要补充

- 固件版本
- Arduino IDE / ESP32 core 版本
- 依赖库
- 引脚表
- RaceChrono 设备名
- 轮位 ID
- 已知问题
- 实测结果
