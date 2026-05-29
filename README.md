# Automotive Telemetry Equipment

汽车赛道遥测设备项目，主要用于赛道场景下的轮胎温度、悬挂高度、刹车盘温度、卡钳温度、车内显示和 RaceChrono Pro 数据记录。

本仓库目标不是做一个临时 Demo，而是沉淀一套可以长期维护、可交付、可复现、方便 Codex/ChatGPT 协助修改的工程项目。

## 当前项目主线

当前项目重点维护两条主线：

1. **胎温高度节点 v3 RaceChrono BLE only**
2. **brake_temp_alarm_display_12V_RS485**

同时保留三个非主线 / 独立 / 历史版本：

1. **brake_temp_12v_max31856**：MAX31856 + K 型热电偶，测卡钳，当前未接入报警显示
2. **brake_temp_12v_rs485**：RS485 红外刹车盘测温初代版本，不带显示报警，不作为当前重点推进对象
3. **胎温高度节点 v2**：历史测试板，仅作参考

## 项目目标

### 1. 胎温高度节点 v3（当前主线）

- ESP32 / ESP32-C3 主控
- MLX90640 红外阵列测轮胎温度
- VL53L1X 悬挂高度 / 离地距离测量
- RaceChrono / RejsaRubberTrac compatible BLE only
- 不再向中控发送胎温数据
- 不再使用 ESP-NOW / WiFi / UDP / TCP 作为胎温主链路
- 单按键电源控制
- 锂电池供电
- Type-C 烧录 / 充电
- 面向客户交付前的稳定性优化

胎温高度 v3 当前最高优先级：

```text
RaceChrono 数据准确
热力分布跟手
温度不过度抖动
BLE 连接稳定
```

### 2. brake_temp_alarm_display_12V_RS485（当前刹车节点板重点版本）

- 使用 RS485 通信方式的红外测温传感器
- 测量目标：**刹车盘温度**
- 车内 OLED / TFT 报警显示
- 显示刹车盘温度和 `ALM` 报警阈值
- SET / UP / DOWN 按键设置报警阈值
- 蜂鸣器超温报警
- 必须支持 RaceChrono Pro BLE 数据发送
- 注意：该版本不是 MAX31856 热电偶版本，也不是初代 `brake_temp_12v_rs485`

### 3. brake_temp_12v_max31856（独立采集/测试版本）

- 使用 MAX31856 + K 型热电偶
- 测量目标：**卡钳温度**
- 当前还没有接入报警显示
- 不等同于 `brake_temp_alarm_display_12V_RS485`
- 重点关注热电偶输入、MAX31856 fault、电源噪声和 EMI

### 4. brake_temp_12v_rs485（初代 RS485 版本）

- 使用 RS485 通信方式的红外测温传感器
- 测量目标：**刹车盘温度**
- 不带显示报警
- 属于 `brake_temp_alarm_display_12V_RS485` 之前的初代版本
- 不作为当前重点推进对象

### 5. 胎温高度节点 v2（历史测试板）

- 历史验证板，仅保留为参考资料
- 不作为当前项目主线继续扩展

## 最高优先级

本项目所有代码和硬件改动都必须优先考虑：

1. 稳定开机
2. 稳定传感器初始化
3. 稳定 BLE 连接 RaceChrono Pro
4. 胎温高度 v3 的 RaceChrono 热力分布跟手且温度不过度抖动
5. brake_temp_alarm_display_12V_RS485 的 RS485 读取、显示、报警和 RaceChrono 发送稳定
6. 真实赛道环境抗震、抗干扰、抗掉线
7. 代码可读、可维护、可回退
8. 客户交付可靠性

不追求花哨功能，不为了理论最优牺牲稳定性。

## 推荐仓库结构

```text
Automotive-telemetry-equipment/
├─ README.md
├─ CODEX.md
├─ AGENTS.md
├─ docs/
│  ├─ 01_项目背景.md
│  ├─ 02_硬件版本说明.md
│  ├─ 03_固件结构与命名.md
│  ├─ 04_RaceChrono_BLE说明.md
│  ├─ 05_已知硬件问题与排查记录.md
│  ├─ 06_测试与交付检查清单.md
│  ├─ 07_Git与Codex协作流程.md
│  ├─ 08_12V_RS485刹车节点说明.md
│  └─ 09_胎温高度v3_RaceChrono_BLE_ONLY说明.md
├─ firmware/
│  ├─ tire_temp_height_node_v2/                  # 历史参考
│  ├─ tire_temp_height_node_v3/                  # 当前 RaceChrono BLE only 主线
│  ├─ brake_temp_12v_max31856/                   # 独立热电偶卡钳测温版本
│  ├─ brake_temp_12v_rs485/                      # 初代RS485红外刹车盘版本，不带显示报警，非重点
│  ├─ brake_temp_alarm_display_12v_rs485/        # 当前刹车节点板重点版本：显示 + 报警 + RaceChrono
│  └─ central_display/
└─ hardware/
   ├─ tire_temp_height_v2/                       # 历史参考
   ├─ tire_temp_height_v3/                       # 当前主线
   ├─ brake_temp_12v_rs485/                      # 初代RS485红外刹车盘版本，不带显示报警，非重点
   └─ brake_temp_alarm_display_12v_rs485/        # 当前刹车节点板重点硬件
```

## 当前建议

- 所有固件文件放入 `firmware/` 对应目录。
- 所有原理图、Gerber、BOM、3D 文件放入 `hardware/` 对应目录。
- 让 Codex 修改代码前，先要求它阅读 `CODEX.md` / `AGENTS.md` / `docs/`。
- 修改胎温高度 v3 前，必须阅读 `docs/09_胎温高度v3_RaceChrono_BLE_ONLY说明.md`。
- 修改刹车节点前，必须先确认目标版本是 `brake_temp_12v_max31856`、`brake_temp_12v_rs485`，还是当前主线 `brake_temp_alarm_display_12v_rs485`。
- 每次让 Codex 改代码时，都要求它输出完整 `.ino` 文件，不要只给片段补丁。

## 重要原则

如果硬件已经验证稳定，软件不应随意改动相关引脚、时序和电源控制逻辑。

尤其是：

- 胎温高度 v3 当前只向 RaceChrono 发送数据，不再向中控发送胎温数据。
- 不要重新加入 ESP-NOW 胎温发送、中控胎温发送、WiFi / UDP / TCP 胎温链路。
- RaceChrono BLE 协议相关逻辑不要随便重构。
- 胎温清洗、ROI 跟踪、trimmed mean、rate limit、adaptive low-pass、spike gate 这些平衡“跟手”和“抖动”的逻辑不要随意删除。
- `brake_temp_alarm_display_12V_RS485` 是当前刹车节点板重点版本，必须保留显示、报警和 RaceChrono BLE 数据发送能力，除非明确要求移除。
- `brake_temp_12v_rs485` 是不带显示报警的初代 RS485 版本，不作为当前重点推进对象。
- `brake_temp_12v_max31856` 是独立热电偶卡钳测温版本，目前还没有接入报警显示。
- 不要把 MAX31856 热电偶卡钳测温逻辑、初代 RS485 逻辑、报警显示 RS485 逻辑互相混用。
- RS485 读取、OLED 显示、蜂鸣器报警不能阻塞 RaceChrono BLE。
- 以后只要修改 BLE DIY CAN 自定义通道相关固件，必须在回复和对应 `.md` 文档中同步写明 PID / CAN ID、字节布局、单位、缩放倍率和 RaceChrono 计算公式；胎温高度 v3 的 RejsaRubberTrac 风格 BLE 热图不是 DIY CAN，不按 PID 规则记录。
- 传感器初始化和失败重试逻辑不要为了简洁删除。
- 电源控制、长按关机、按键识别逻辑必须保守修改。
- 出现偶发问题时，优先增加日志和保护逻辑，不要盲目大改架构。
