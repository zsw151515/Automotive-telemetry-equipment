# CODEX.md

本文件用于告诉 Codex / ChatGPT：本仓库是什么项目、硬件背景是什么、哪些地方不能乱改、修改代码时必须遵守什么规则。

在本仓库工作时，请优先阅读：

1. `README.md`
2. `CODEX.md`
3. `AGENTS.md`
4. `docs/01_项目背景.md`
5. `docs/02_硬件版本说明.md`
6. `docs/05_已知硬件问题与排查记录.md`
7. `docs/08_12V_RS485刹车节点说明.md`
8. `docs/09_胎温高度v3_RaceChrono_BLE_ONLY说明.md`

## 1. 项目一句话说明

这是一个用于赛道场景的汽车遥测设备项目，包括：

- 胎温监测
- 悬挂高度 / 离地距离监测
- 刹车盘温度监测
- 卡钳温度监测
- RaceChrono Pro BLE 数据记录
- RS485 / 中控显示
- 面向真实客户交付的硬件和固件稳定化

本项目优先级是：**可靠、稳定、可交付，而不是功能复杂。**

## 2. 当前项目主线

当前主线只有两个：

```text
1. 胎温高度节点板 v3 RaceChrono BLE only
2. break_temp_alarm_display_12V_RS485
```

历史 / 独立版本：

```text
1. 胎温高度节点板 v2：历史测试板，仅作参考
2. brake_temp_12v_max31856：独立热电偶卡钳测温版本，当前未接入报警显示
```

## 3. 代码修改总原则

Codex 修改本仓库代码时，必须遵守以下原则：

1. 优先保证已有功能不坏。
2. 不要随意重构已经稳定运行的 BLE / 传感器 / 电源控制逻辑。
3. 不要随意改引脚定义。
4. 不要删除硬件排查有用的串口日志。
5. 不要为了代码“简洁”删掉失败重试、传感器检查、保护逻辑。
6. 不要把稳定代码改成复杂架构。
7. 修改 `.ino` 文件时，用户偏好是提供**完整终版代码**，而不是零散补丁。
8. 每次修改必须说明：改了什么、为什么改、保留了什么、需要怎样测试。

## 4. 固件风格要求

固件以 Arduino / ESP32 环境为主。

推荐代码风格：

- 文件顶部写清楚硬件版本、板子型号、传感器型号、用途。
- 所有引脚集中定义。
- 所有用户可调参数集中定义。
- 保留清晰的串口启动日志。
- 传感器初始化必须打印成功 / 失败原因。
- BLE 连接 / 断开必须有日志。
- 按键逻辑必须有去抖。
- 主循环尽量非阻塞。
- 报警、显示、通信逻辑要分模块函数处理。
- 避免大量 `delay()` 阻塞 BLE 或传感器读取。

## 5. 胎温高度节点 v3 当前软件定位

胎温高度节点 v3 是当前重要的量产候选版本。

当前 v3 软件主线已经改为：

```text
RaceChrono / RejsaRubberTrac compatible BLE only
```

也就是说：

```text
MLX90640 胎温 + VL53L1X 高度
        ↓
本地清洗 / 滤波 / ROI / 分区
        ↓
BLE RejsaRubberTrac 风格数据包
        ↓
RaceChrono Pro 记录和显示
```

不再使用：

```text
ESP-NOW -> 中控发送胎温
WiFi -> 中控
UDP/TCP -> RaceChrono
Central display link
```

Codex 修改胎温高度 v3 时，必须优先保证：

1. RaceChrono 能稳定搜索、连接、记录。
2. 胎温热力分布要跟手。
3. 温度不能有很夸张的抖动。
4. MLX90640 错误恢复和 FAST/STABLE 回退逻辑不能删除。
5. 不要为了中控显示或 ESP-NOW 功能牺牲 BLE 质量。

当前参考固件：

```text
tire_temp_height_node_racechrono_fast_clean_v3.ino
```

该固件启动日志明确：

```text
BLE only. No WiFi, no ESP-NOW, no central display link.
```

## 6. 胎温高度节点 v3 关键硬件背景

主要器件：

- ESP32-C3 模块 / 板
- MLX90640 红外阵列温度传感器
- VL53L1X 距离传感器
- LTC2954 单按键电源控制
- ME6211C33M5G-N 3.3V LDO
- TP4057 锂电池充电
- Type-C 接口
- USBLC6-2SC6 USB ESD

已验证的重要硬件结论：

- `LTC2954CTS8-1#TRPBF` 不适合当前最终电源保持逻辑。
- 后续 v3 / v3.1 应固定使用 `LTC2954CTS8-2#TRPBF`。
- `R_BTN_PULL` 应删除或 DNP。
- `PWR_KILL -> SYS_3V3` 需要增加 47kΩ 上拉。
- 上述组合已通过电池供电连续约 30 次开机测试，未再出现偶发开机失败。

重要警告：

- 不要随意修改 `PWR_KILL`、`PWR_INT`、按键、电源保持相关逻辑。
- 不要把已经实测稳定的电源修复方案又改回旧方案。

## 7. 胎温高度节点 v3 常用引脚背景

典型 v3 固件引脚：

```cpp
const int SDA_PIN          = 8;
const int SCL_PIN          = 10;
const int VBAT_ADC_PIN     = 1;
const int LED_STAT_PIN     = 2;  // 低电平点亮
const int BTN_PIN          = 3;  // 低电平有效
const int PWR_KILL_PIN     = 4;  // HIGH 保持电源
const int PWR_INT_PIN      = 5;
const int VL53_XSHUT_PIN   = 6;
const int VL53_INT_PIN     = 7;
```

除非用户明确要求，不要改变这些引脚。

## 8. 胎温高度节点 v3 BLE / 温度处理规则

当前满意固件使用 RejsaRubberTrac 风格 BLE：

```text
Device name: RejsaRubberRL123456
Service UUID: 0x1FF7
Characteristic 1: 0x0001
Characteristic 2: 0x0002
Characteristic 3: 0x0003
```

不要随意改 BLE 设备名、Service、Characteristic 和数据包布局。

当前满意固件的温度处理思路：

```text
有效范围剔除
trimmed mean 分区均值
ROI 自动跟踪
rate limit 限制变化速度
adaptive low-pass 自适应低通
spike gate 抑制离谱跳变
stale 温度保护
```

不要随意删除这些逻辑。它们是“热力分布跟手”和“温度不过度抖动”的平衡点。

当前满意固件优先 FAST profile：

```text
1MHz I2C
MLX90640 32Hz
17bit ADC
约 34ms frame interval
```

若总线不稳，应自动回退 STABLE profile：

```text
400kHz I2C
MLX90640 16Hz
18bit ADC
约 70ms frame interval
```

不要删除 recover / fallback 逻辑。

## 9. 胎温高度节点 v2 背景

v2 是历史测试板，重点用于参考：

- MLX90640 胎温
- VL53L1X 高度
- BLE RaceChrono
- 早期 ESP-NOW 到中控
- TFT 中控显示 ACK 控制

v2 仅作为历史参考，不是当前主线。

## 10. RaceChrono Pro 集成原则

RaceChrono Pro，尤其是 iOS 版本，是本项目核心目标。

重要结论：

- 胎温热力图优先使用 RejsaRubberTrac 风格 BLE 设备。
- BLE 设备名、服务、特征和数据布局不要随意改。
- iOS RaceChrono 对自定义通道支持有限，不要轻易切到 UDP/TCP 外部遥测方案。
- BLE 连接稳定性优先于显示花哨程度。

修改 RaceChrono 相关代码前，必须先说明是否会影响 RaceChrono 搜索设备、连接设备、显示热图或记录数据。

## 11. brake_temp_12v_max31856 版本规则

`brake_temp_12v_max31856` 是独立版本，不是 RS485 报警显示版本。

定位：

```text
MAX31856 + K 型热电偶
测量目标：卡钳温度
当前状态：还没有接入报警显示
```

Codex 修改此版本时必须遵守：

1. 不要把它当成 `break_temp_alarm_display_12V_RS485`。
2. 不要把卡钳温度误写成刹车盘温度。
3. 保留 MAX31856 fault 读取和日志。
4. 保留热电偶断线 / fault / NaN / 异常值判断。
5. 当前未接入报警显示，除非用户明确要求，不要擅自加入报警显示架构。
6. 不要把异常读数显示或发送为正常 `0°C`。

## 12. break_temp_alarm_display_12V_RS485 版本规则

`break_temp_alarm_display_12V_RS485` 是当前主线之一。

定位：

```text
RS485 红外测温传感器
测量目标：刹车盘温度
功能：显示 + 报警 + RaceChrono BLE 数据发送
```

当前使用的 RS485 红外测温传感器为固定式红外测温仪，采用 Modbus RTU 通信，默认地址 0x01，默认 9600 波特率。温度读取使用功能码 0x03，寄存器地址 0x0000，返回值除以 10 为摄氏度。

Codex 修改此版本时必须遵守：

1. 不要把它当成 `brake_temp_12v_max31856`。
2. 不要把刹车盘温度误写成卡钳温度。
3. 不要加入 MAX31856 热电偶采集逻辑，除非用户明确要求一体化。
4. 必须保留 RS485 Modbus RTU 读取逻辑。
5. 必须保留 OLED / TFT 显示和 ALM 报警阈值逻辑。
6. 必须保留蜂鸣器报警逻辑。
7. 必须支持 RaceChrono BLE 数据发送，除非用户明确要求移除。
8. RS485 读取、OLED 刷新、按键扫描、蜂鸣器任务不能阻塞 BLE。
9. 不要把 RS485 读取失败、CRC 错误、温度无效显示或发送为正常 `0°C`。
10. 修改后必须说明 RaceChrono 是否可搜索、可连接、发送哪些通道。
11. 修改后必须说明蜂鸣器逻辑，尤其是 SET 长按静音后再次超温是否会响。

## 13. 中控显示背景

中控显示目标是车内可读、延迟低、报警明确。

显示代码应优先考虑：

- 大字体
- 高对比度
- 少信息干扰
- 状态清楚
- 报警阈值显示清楚
- 按键响应及时
- 蜂鸣器逻辑明确

用户经常会要求完整终版显示代码，不要只给局部函数。

## 14. 已知硬件坑

### 14.1 Type-C 母座动态接触问题

曾出现一块 v3 新贴片板 Windows 提示“无法识别 USB 设备”。

当时已检查：

- 3.3V 正常
- EN / BOOT / IO8 正常
- CC 电阻正常
- D+ / D- 静态导通正常
- 更换 MCU 无效
- 拆 USBLC6 无效

最终通过重新贴焊 Type-C 母座解决。

结论：

**Type-C 母座即使万用表静态导通正常，也可能存在动态接触或焊接质量问题。**

### 14.2 I2C 串联电阻误焊

I2C 串联电阻位置如果误焊为 10kΩ，会导致传感器无法通信。

正确方向：

- I2C 串联电阻通常应为 0Ω / 22Ω / 47Ω 级别
- 不应是 10kΩ
- I2C 上拉才是 4.7kΩ / 10kΩ 级别

## 15. 给 Codex 的工作方式

当用户要求修改代码时，应按以下格式回复：

1. 先确认修改目标。
2. 说明涉及的文件。
3. 给出完整终版代码。
4. 说明关键改动。
5. 给出烧录和测试步骤。
6. 给出异常排查建议。

不要说“我稍后完成”。必须在当前回复中完成尽可能多的工作。

## 16. 严禁事项

以下操作除非用户明确要求，否则不要做：

- 删除 RaceChrono BLE 代码
- 修改 BLE 设备名规则
- 修改轮位 ID 规则
- 修改 v3 电源控制引脚
- 删除传感器初始化失败日志
- 删除 I2C 扫描 / 诊断日志
- 把所有故障读数强行显示或发送为 0
- 把胎温高度 v3 改回向中控发送胎温数据
- 给胎温高度 v3 重新加入 ESP-NOW 胎温链路、中控胎温链路、WiFi / UDP / TCP 胎温主链路
- 删除胎温滤波、ROI 跟踪、spike gate、rate limit、FAST/STABLE fallback 逻辑
- 把 `break_temp_alarm_display_12V_RS485` 改成不支持 RaceChrono 的纯显示版本
- 把 MAX31856 热电偶卡钳测温逻辑混入 RS485 红外刹车盘显示版本
- 把 RS485 红外刹车盘测温逻辑混入 MAX31856 卡钳测温版本
- 大规模重构稳定代码
- 引入复杂第三方库导致 Arduino 编译风险增加
