# Automotive Telemetry Equipment

汽车赛道遥测设备项目，主要用于赛道场景下的轮胎温度、悬挂高度、刹车温度、车内显示和 RaceChrono Pro 数据记录。

本仓库目标不是做一个临时 Demo，而是沉淀一套可以长期维护、可交付、可复现、方便 Codex/ChatGPT 协助修改的工程项目。

## 项目目标

本项目当前围绕以下设备展开：

1. **胎温高度节点 v2 / v3**
   - ESP32 / ESP32-C3 主控
   - MLX90640 红外阵列测温
   - VL53L1X 悬挂高度 / 离地距离测量
   - BLE 连接 RaceChrono Pro
   - ESP-NOW 发送数据到中控显示

2. **胎温高度节点 v3 量产候选硬件**
   - 单按键电源控制
   - 锂电池供电
   - Type-C 烧录 / 充电
   - 面向客户交付前的稳定性优化

3. **12V 刹车温度节点**
   - 汽车 12V 供电
   - MAX31856 + K 型热电偶
   - 目标用于赛道刹车盘 / 卡钳附近温度监测
   - 支持 RS485 发送到车内报警显示中控
   - 必须支持 RaceChrono Pro BLE 数据记录

4. **12V RS485 刹车报警显示中控**
   - 通过 RS485 读取刹车温度节点
   - OLED / TFT 显示刹车温度和 ALM 报警阈值
   - SET / UP / DOWN 按键设置报警阈值
   - 蜂鸣器超温报警
   - 小屏幕版本优先大字体、少信息、明确 FAIL 状态

5. **中控显示模块**
   - ESP32 TFT / OLED 显示
   - 显示轮胎温度、刹车温度、报警阈值、连接状态等
   - 重点要求车内可读性、低延迟、稳定显示

## 最高优先级

本项目所有代码和硬件改动都必须优先考虑：

1. 稳定开机
2. 稳定传感器初始化
3. 稳定 BLE 连接 RaceChrono Pro
4. 稳定 RS485 通信和本地报警显示
5. 真实赛道环境抗震、抗干扰、抗掉线
6. 代码可读、可维护、可回退
7. 客户交付可靠性

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
│  └─ 08_12V_RS485刹车节点说明.md
├─ firmware/
│  ├─ tire_temp_height_node_v2/
│  ├─ tire_temp_height_node_v3/
│  ├─ brake_temp_12v_max31856/
│  ├─ brake_temp_12v_rs485_racechrono_node/
│  ├─ brake_temp_alarm_display_12v_rs485/
│  └─ central_display/
└─ hardware/
   ├─ tire_temp_height_v2/
   ├─ tire_temp_height_v3/
   └─ brake_temp_12v_rs485/
```

## 当前建议

- 所有固件文件放入 `firmware/` 对应目录。
- 所有原理图、Gerber、BOM、3D 文件放入 `hardware/` 对应目录。
- 让 Codex 修改代码前，先要求它阅读 `CODEX.md` / `AGENTS.md` / `docs/`。
- 修改 12V RS485 刹车节点前，必须阅读 `docs/08_12V_RS485刹车节点说明.md`。
- 每次让 Codex 改代码时，都要求它输出完整 `.ino` 文件，不要只给片段补丁。

## 重要原则

如果硬件已经验证稳定，软件不应随意改动相关引脚、时序和电源控制逻辑。

尤其是：

- RaceChrono BLE 协议相关逻辑不要随便重构。
- 12V RS485 刹车节点必须保留 RaceChrono BLE 数据发送能力，除非明确要求移除。
- RS485 读取、OLED 显示、蜂鸣器报警不能阻塞 RaceChrono BLE。
- 传感器初始化和失败重试逻辑不要为了简洁删除。
- 电源控制、长按关机、按键识别逻辑必须保守修改。
- 出现偶发问题时，优先增加日志和保护逻辑，不要盲目大改架构。
