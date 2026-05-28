# brake_temp_alarm_display_12V_RS485 固件

## 定位

本目录用于当前主线之一：`brake_temp_alarm_display_12V_RS485`。

它和 `brake_temp_12v_max31856` 不是同一个版本，不能混淆。

## 测量目标

```text
刹车盘温度
```

## 传感器类型

```text
RS485 通信方式的红外测温传感器
```

当前上传的传感器手册显示，该类固定式红外测温仪支持 RS485 Modbus RTU 输出，默认地址 0x01，默认 9600 波特率，温度值读取后需要除以 10 得到摄氏度。

## 主要功能

- 通过 RS485 Modbus RTU 读取红外测温传感器
- OLED / TFT 显示当前刹车盘温度
- 显示 `ALM` 报警阈值
- SET / UP / DOWN 按键设置阈值
- 蜂鸣器超温报警
- 传感器或通信失败时显示 FAIL
- 必须支持 RaceChrono Pro BLE 数据发送

## 当前固件行为记录

当前固件文件：

```text
hyundai_n_brake_temp_alarm_display_12V_RS485.ino
```

当前定位：

```text
V1.5 Thermal Load Alarm + RaceChrono BLE
```

关键行为：

- 显示仍使用 V1.4 Hyundai N OLED 开机动画，未改动画代码。
- 屏幕主数字显示当前有效刹车盘温度最大值。
- 报警不只看瞬时盘面温度，而是使用 `Tfast / Tcore / Dose` 热负荷模型，减少单次温度尖峰误报。
- RS485 失败时不会把温度显示或发送为正常 `0°C`。
- RS485 候选寄存器探测按轮次逐个尝试，避免传感器断开时一次轮询阻塞太久。
- 按键提示蜂鸣改为非阻塞，不应造成 OLED 刷新或 RaceChrono 发送明显停顿。
- RaceChrono 使用 BLE DIY CAN-Bus API，设备名为 `N BrakeTemp`。
- RaceChrono 默认发送节奏为约 150ms 一轮，兼顾流畅和 BLE 稳定。

RaceChrono DIY CAN PIDs：

```text
0x100 Brake FL        int16 BE, 温度 x10 C，0x7FFF 表示无效
0x101 Brake FR        int16 BE, 温度 x10 C，0x7FFF 表示无效
0x102 Brake Display   int16 BE, 屏幕显示温度 x10 C
0x103 Brake Fast      int16 BE, Tfast x10 C
0x104 Brake Core      int16 BE, Tcore x10 C
0x105 Brake Dose      uint16 BE, Dose x10
0x106 Brake State     uint8, 0 OK / 1 WARN / 2 ALARM / 3 FAIL
0x107 Brake Alarm     uint16 BE, ALM C
```

蜂鸣器逻辑：

- `SET` 长按切换的是 FAIL / WARN 提示蜂鸣。
- 真正 `ALARM` 状态仍会强制蜂鸣，除非用户使用 `UP + DOWN` 长按进入 60 秒静音。
- 60 秒静音结束后，如果仍处于 ALARM，会再次蜂鸣。
- RaceChrono 断开不影响本地报警显示和蜂鸣器逻辑。

## 当前状态

```text
当前状态：当前主线 / 待上传稳定固件
是否当前报警显示主线：是
是否 MAX31856 热电偶版本：否
```

## UI 原则

小屏幕版本，例如 1.3 寸 OLED：

- 主温度数字要尽量大
- `ALM` 和 `SET` 逻辑保留
- 不要堆太多状态文字
- FAIL 状态必须明确
- 按键响应必须及时
- RaceChrono / BLE 状态可选，但不要挤占主温度显示

## Codex 修改规则

- 不要把本版本当成 `brake_temp_12v_max31856`。
- 不要把刹车盘温度写成卡钳温度。
- 不要混入 MAX31856 热电偶采集逻辑，除非用户明确要求一体化。
- 不要把 RS485 失败显示为 0°C。
- 不要让显示刷新阻塞按键。
- 不要让 RS485 读取阻塞蜂鸣器逻辑。
- 不要让 RS485 / 显示 / 按键阻塞 RaceChrono BLE。
- 修改后必须说明蜂鸣器逻辑。
- 修改后必须说明 RaceChrono BLE 是否可搜索、可连接、发送哪些通道。

## 上传固件后需要补充

- 显示屏型号
- I2C / SPI 引脚
- RS485 UART 引脚
- RS485 DE/RE 控制引脚
- RS485 传感器地址
- Modbus RTU 参数
- 按键引脚
- 蜂鸣器引脚
- 报警阈值保存方式
- RaceChrono BLE 设备名
- RaceChrono 通道
- 当前 UI 版本说明
