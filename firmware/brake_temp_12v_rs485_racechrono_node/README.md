# 12V RS485 + RaceChrono 刹车温度采集节点固件

## 用途

本目录用于 12V 刹车温度采集节点固件。

该版本必须同时支持：

1. MAX31856 读取 K 型热电偶刹车温度
2. RS485 输出到车内报警显示中控
3. RaceChrono Pro BLE 输出刹车温度数据

## 架构

```text
MAX31856 读取温度
        ↓
异常判断 / fault 判断 / 滤波
        ↓
同时输出：
  RS485 -> 报警显示中控
  BLE   -> RaceChrono Pro
```

## 当前状态

```text
当前状态：待上传稳定固件
```

## Codex 修改规则

- 不要把本版本改成纯 RS485 显示版本。
- 不要删除 RaceChrono BLE 广播和数据发送。
- 不要删除 MAX31856 fault 日志。
- 不要把异常温度显示或发送为 0°C。
- 不要让 RS485 / OLED / 按键阻塞 BLE。
- 修改后必须说明 RaceChrono 是否可搜索、可连接、发送哪些通道。

## 上传固件后需要补充

- MCU 型号
- MAX31856 SPI 引脚
- RS485 UART 引脚
- RS485 DE/RE 控制引脚
- RS485 地址
- RaceChrono BLE 设备名
- RaceChrono 通道 / PID / CAN ID
- 发送频率
- 蜂鸣器逻辑
- 实测问题
