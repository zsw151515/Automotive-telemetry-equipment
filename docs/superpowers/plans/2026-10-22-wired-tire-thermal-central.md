# 四路有线胎温中控 Implementation Plan

最新范围：用户选择六接口（四胎温＋两前刹车）、7B 触屏和底板同壳；刹车采集/报警/BLE 并入底板，触屏替代 OLED/设置键、保留蜂鸣。后续需先审阅[六接口设计](../specs/2026-10-24-six-port-tire-brake-7b-design.md)并编写增量计划，本文件不包含新刹车控制器、设置命令或触屏的实施步骤。

2026-10-24：用户选定 7B 完整显示套件＋四接口采集底板、同一外壳。后续显示与硬件工作先依据[设计变更说明](../specs/2026-10-24-wired-tire-7b-kit-design.md)完成审阅和增量计划；本计划中关于自行设计 P4/屏幕同板的结构步骤保留作历史，不直接执行。离线软件验证证据仍只属于原交付版本。

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现一块带 7 英寸屏、四路 RS485 成品胎温探头接口、四轮独立 RaceChrono BLE 输出的定制中控，并取得协议、通信、显示与供电的验证记录。

**Architecture:** 一个显示主控与四个采集/BLE 单元集成在同一块 PCB。每个采集单元读取一路完整温度矩阵、计算八分区并独立发送 BLE，显示主控通过板内 SPI 获取完整快照。工作按协议、单轮、四轮、显示和正式硬件依次推进，禁止跨过实物证据要求直接投板。

**Tech Stack:** ESP32-C3 ×4、ESP32-P4 ×1；首选复用 Arduino-ESP32 3.3.7 与其内置 ESP-IDF 驱动；C++17 可移植协议/算法库；Python 3 串口抓包；P4 `esp_lcd` MIPI-DSI；独立原生 C++ 测试。必要的面板厂商驱动以选定面板的正式资料为准。

**Spec:** [四路有线胎温中控设计规格](../specs/2026-10-22-wired-tire-thermal-central-design.md)，用户于本次会话同意继续进入计划阶段。

## 离线执行状态（2026-10-23）

用户选择原生执行，明确没有实物条件，要求先完成离线软件。任务 1–5、7–9 的软件部分已实现；任务 2–6、8–11 的实物验证继续保持未通过。任务 12 交付为离线研发包，默认不启用未核准 I/O，不能用于直接接线或投板。原任务中的实物勾选项保留未勾选，避免将离线进度当成硬件通过。详细证据见 `hardware/wired_tire_central/notes/validation_offline.md`。

## Global Constraints

- 一块中控 PCB 集成一个显示主控与四个独立采集/BLE 单元；使用定制板，不将带屏开发套件作为交付结构。
- 中控本地必须显示四幅完整 32×24 热图；RaceChrono 保留现有风格的 8 分区热图。
- 四个接口分别为 FL、FR、RL、RR；前轮线束约 2 m，后轮约 4.5 m。
- 不采集悬挂高度，不使用轮端电池；车辆点火上电后自动工作，关闭点火后关闭。
- BLE 为 RejsaRubberTrac 风格，服务 `0x1FF7`、特征 `0x0001/0x0002/0x0003`，各 20 字节；不使用 DIY CAN PID。
- 温度计数统一为 `int16_t`、0.1°C；传感器寄存器大端，BLE 温度字段小端；字节序不能依赖结构体内存布局。
- 运行串口基线 115200、8N2；须先只读识别实物配置，禁止自动盲写或复位探头。
- 1500 ms 为数据过期基线；界面绘制目标 30 FPS，新测温帧率单独记录。
- 新方向独立于无线 v3 和刹车报警节点；既有固件、引脚和协议不作为本计划的修改对象。
- `.ino` 必须交付完整文件，含 includes、全局变量、setup、loop；配套库和安装步骤一并交付。
- 软件仿真、原生测试、编译、真实传感器测试和实车测试分别报告，不能相互替代。

## Review Focus

1. CRC 正确但地址、功能码、声明长度或寄存器跨度错误的响应不能覆盖有效温度；由任务 1、2 验证。
2. 真实 0°C、负温和静止场景的相同连续图像不能被误判为故障或新采样；由任务 1、3、4 验证。
3. 一个 MCU 重启、序号回绕、不同 MCU 时钟以及“传输仍成功但内容已旧”不能延长有效期；由任务 7、8 验证。
4. 四个独立 BLE 身份集中在屏幕后时不能串轮位；传感器失效后手机不得被误认为仍在接收实时温度；由任务 5、6、11 验证。
5. 启动压降、Flash 配置写入期间断电、单个探头支路短路不能造成其余轮位伪正常或系统无法恢复；由任务 4、10、11 验证。

---

## 执行边界与当前环境

本计划只在用户审阅并选择执行方式后实施。协议库、采集固件、显示固件和硬件共享确定的数据契约，存在严格前后依赖，因此作为一条集成计划推进，不并行冻结各自接口。

2026-10-22 只读检查结果：本机 Arduino CLI 1.5.0 位于 `E:/arduino/Arduino IDE/resources/app/lib/backend/resources/arduino-cli.exe`，已装 `esp32:esp32` 3.3.7，包含 `esp32:esp32:esp32c3` 和 `esp32:esp32:esp32p4` 目标。当前 shell 找不到 `idf.py`、CMake、CTest、g++ 或 clang++；找不到不代表磁盘上一定未安装。先定位可用原生工具链，确实缺少时再在执行阶段安装，不在计划编写阶段安装。

首选使用已安装 Arduino core 完成两个目标，避免为了 P4 预先引入另一套完整环境。若面板驱动在此 core 上无法构建，记录具体错误后再评估 ESP-IDF 路线；切换构建路线须更新计划和交付说明，不自动升级旧项目依赖。

所有命令默认在仓库根目录执行。`$probePort`、`$channelPort` 是执行时通过端口列表与实物对应后设置的任务变量，不能猜测 COM 号。抓包时间戳为接收时间，不能标成传感器曝光时间。

## 文件分工

以下是后续创建/修改清单，不代表文件已存在：

| 文件或目录 | 职责 |
| --- | --- |
| `firmware/libraries/WiredTireCore/library.properties`、`src/WiredTireCore.h` | Arduino 可安装的共享 C++ 库与公共入口 |
| 同目录 `src/ThermalFrame.h`、`src/Yf90640Protocol.h/.cpp` | 固定尺寸数据类型、寄存器解析及 CRC |
| 同目录 `src/ProbeSchedule.h/.cpp` | 有界请求、重试和数据过期调度 |
| 同目录 `src/TireZones.h/.cpp`、`src/RaceChronoPacket.h/.cpp` | 分区算法与纯字节编码，独立于硬件驱动 |
| 同目录 `src/SnapshotWire.h/.cpp`、`src/ThermalView.h/.cpp` | SPI 数据契约、年龄计算与显示模型 |
| `firmware/wired_tire_channel/wired_tire_channel.ino`、`ChannelConfig.h`、`ProbeLink.h/.cpp`、`RaceChronoBle.h/.cpp`、`SnapshotSlave.h/.cpp`、`README.md` | 完整 C3 单轮采集/BLE 固件，四份实例仅由配置区分 |
| `firmware/wired_tire_display/wired_tire_display.ino`、`DisplayConfig.h`、`PanelDriver.h/.cpp`、`SnapshotMaster.h/.cpp`、`HeatmapRenderer.h/.cpp`、`README.md` | 完整 P4 显示固件及硬件适配 |
| `tests/wired_tire/test_*.cpp`、`test_capture_yf90640.py`、`fixtures/README.md` | 真实边界测试及采集样本来源说明 |
| `scripts/capture_yf90640.py` | 只读抓包与端口列举，不实现写配置命令 |
| `scripts/test_wired_tire_core.ps1`、`scripts/compile_wired_tire.ps1`、`scripts/package_wired_tire.ps1` | 原生测试、按目标编译及完整交付打包 |
| `docs/11_四路有线胎温中控接口.md` | 已验证的探头参数、SPI 字段表、BLE 全字段、配置及版本说明 |
| `hardware/wired_tire_central/README.md`、`notes/validation_*.md`、`notes/toolchain.md`、`notes/pinmap.md`、`notes/screen_selection.md` | 实测记录、依赖版本、引脚和面板选型 |
| 同目录 `schematic/`、`pcb/`、`bom/`、`gerber/`、`3d/`、`notes/harness.md` | 原理图源文件、PCB、制造输出、机械模型和线束表 |

不将生成的抓包原始大文件、构建目录或安装程序直接加入 Git；小型回归样本须注明设备版本、获取方式及实际/合成属性。合成样本不能冒充传感器实测。

下文 `src/` 唯一指向 `firmware/libraries/WiredTireCore/src/`；`channel`、`display` 分别指上表两个完整 sketch 目录；`notes/` 唯一指向 `hardware/wired_tire_central/notes/`。测试脚本 `-Case` 接受 `all/yf90640/zones/schedule/racechrono/snapshot/view/heatmap`，逐项输出断言结果；每项成功输出 `PASS wired_tire <case>`，全部成功返回 0，编译失败、断言失败或缺少测试返回非零。原生测试不依赖 Arduino 硬件头文件。

## Task 1: 可测试的温度类型与探头响应解析

**Files:** Create `firmware/libraries/WiredTireCore/library.properties`、`src/WiredTireCore.h`、`src/ThermalFrame.h`、`src/Yf90640Protocol.h/.cpp`；Create `tests/wired_tire/test_yf90640.cpp`、`scripts/test_wired_tire_core.ps1`。

**Interfaces:** 命名空间统一为 `wired_tire`。定义 `WheelId`（FL=1、FR=2、RL=3、RR=4）、`FrameState`（未就绪/有效/部分有效/过期/通信失败）、`ThermalFrame`（768 个 `int16_t` 像素与 96 字节有效位图）、`FrameMeta`（轮位、启动标识、接收序号、接收时间和状态）、`ZoneResult`（`std::array<int16_t,8> temp10`、`uint8_t validMask`、ROI 起始行、镜像和配置版本）。提供 `uint16_t modbusCrc(const uint8_t*, size_t)`、`ParseResult decodeYf65(const uint8_t*, size_t, uint8_t expectedAddress, ThermalFrame&)`。失败时输出对象保持不变，返回明确的长度/CRC/地址/功能码错误。枚举的线上数值与头文件定义共用同一来源。

- [ ] 定位原生 C++17 编译器；测试脚本支持 `-CompilerPath`，编译产物放入专属临时目录。缺少编译器必须报缺少工具，不能返回测试通过。
- [ ] 写 `test_yf90640`：CRC 标准向量 `123456789 -> 0x4B37`；1570 字节寄存器数据对应 1576 字节响应；像素起点寄存器 `0x0010`，大端 `FF 9C` 解码为 -100 个计数，零计数保留；首尾像素位置正确。

  ```cpp
  assert(modbusCrc(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0x4B37);
  ```

- [ ] 增加 CRC 正确但地址错误、功能码错误、异常响应、字节数与实际不符、超过 2048 字节接收上限、截断帧测试；断言错误响应不修改旧数据。运行 `powershell -File scripts/test_wired_tire_core.ps1 -Case yf90640`，先记录失败。
- [ ] 实现类型和解析；按经核对的寄存器跨度检查响应，不只检查 CRC。运行同一命令，确认全部断言通过。
- [ ] 为 `test_yf90640` 增加 `--capture <单帧二进制文件>` 重放入口，脚本用 `-CapturePath` 转交；输出解码后的首尾像素及 CRC/长度结果。该入口供任务 2 使用，不把 JSONL 元数据当成二进制帧读取。复跑通过后提交任务文件，提交信息 `firmware: 添加有线胎温帧解析核心`。

## Task 2: 只读抓包工具与单探头实物验证（关卡 A）

**Files:** Create `scripts/capture_yf90640.py`、`tests/wired_tire/test_capture_yf90640.py`、`tests/wired_tire/fixtures/README.md`、`hardware/wired_tire_central/notes/validation_probe.md`、`notes/toolchain.md`、`docs/11_四路有线胎温中控接口.md`。

**Interfaces:** CLI `capture_yf90640.py --list-ports`；采集参数为 `--port --baud --stopbits --seconds --output`，缺少端口或配置参数时报错。输出 JSONL 每行包含 `monotonic_ns`、`direction`、`hex`，旁置设备/配置元数据。仅允许向已选端口发送 `0x03`、`0x65` 读取请求。

- [ ] 用 `unittest` 和串口替身测试只发送读取、读取超时有界、USB 断开后关闭端口、响应分片不丢字节、输出数据与原始串口一致。先运行 `python -m unittest discover -s tests/wired_tire -p test_capture_yf90640.py` 取得失败，再实现 CLI 并复跑通过。

  ```python
  self.assertTrue(all(frame[1] in (0x03, 0x65) for frame in fake_serial.writes))
  ```

- [ ] 执行时核对探头型号、USB-RS485 转换器及供电，列举端口，匹配实物后只读识别 9600/115200 与停止位。Python 串口依赖版本记入 `notes/toolchain.md`，依赖缺失时明确安装记录。
- [ ] 运行 `python scripts/capture_yf90640.py --port $probePort --baud 115200 --stopbits 2 --seconds 60 --output <仓库外采集目录>`；先确认该配置已由上一项识别。用核心解码器比对厂商上位机、边缘像素、负温编码和全帧偏移。
- [ ] 通过静态及移动热源区分成功响应率、可观察图像更新率与端到端响应；检查快照一致性及探头失效时输出。记录手册四类矛盾的实物结果；不能只用连续响应次数声称采样率。
- [ ] 将一份去除无关设备信息的小型真实帧作为回归样本；从 JSONL 中按已验证帧边界提取单帧二进制，用 `test_wired_tire_core.ps1 -Case yf90640 -CapturePath <样本.bin>` 重放。更新接口文档的已验证参数并提交 `docs: 记录成品胎温探头协议验证`。无探头/无抓包时关卡保持未通过，不填写虚构结果；只允许继续不依赖实物参数的离线任务。

## Task 3: ROI、八分区与时间滤波

**Files:** Create `src/TireZones.h/.cpp`、`tests/wired_tire/test_zones.cpp`；Modify `docs/11_四路有线胎温中控接口.md`。

**Interfaces:** 定义 `ZoneConfig` 和 `ZoneFilterState`，提供 `ZoneResult updateZones(const ThermalFrame&, uint64_t rxUs, const ZoneConfig&, ZoneFilterState&)`。输入矩阵只读；状态按轮位独立；无有效区返回对应有效位为零，不用正常温度补齐。

- [ ] 写测试：8 个区各 4 列、8 行 ROI、搜索起始行 3–17；均匀有效 0°C 输入输出八个 0 且全部有效；全部无效像素时全部无效；镜像后八分区顺序反转；输入像素不被改写。

  ```cpp
  assert(updateZones(zeroFrame, 250000, config, filter).validMask == 0xFF);
  ```

  此例 `zeroFrame` 为全部像素有效、温度计数均为零的测试输入，`config/filter` 使用该轮的初始配置和独立状态。
- [ ] 写两轮独立状态、同一静态图重复响应、时间回退/重复时间戳、长时间间隔后恢复、非法浮点中间结果测试。运行 `powershell -File scripts/test_wired_tire_core.ps1 -Case zones`，记录失败。
- [ ] 从旧 v3 参考算法移植纯逻辑，保留 -20 至 200°C 分区有效域和保护顺序，滤波以实测 `dt` 计算；配置集中管理。逐帧 ROI 限速若需改成按时间限制，先加入对应测试并在接口文档写出变化理由。
- [ ] 复跑 `zones` 及 `yf90640`，再用关卡 A 的热源记录评价跟手性与抖动；提交 `firmware: 添加有线胎温八分区处理`。

## Task 4: 完整单轮采集固件与恢复状态机

**Files:** Create `firmware/wired_tire_channel/wired_tire_channel.ino`、`ChannelConfig.h`、`ProbeLink.h/.cpp`、`README.md`、`scripts/compile_wired_tire.ps1`；Create `src/ProbeSchedule.h/.cpp`、`tests/wired_tire/test_schedule.cpp`；Modify `firmware/README.md`、`notes/pinmap.md`、`docs/11_四路有线胎温中控接口.md`。

**Interfaces:** `ProbeSchedule` 接收单调时间及响应结果，输出读请求/等待/重试/过期事件；提供 `bool isFresh(uint64_t nowUs, uint64_t lastGoodUs)`，数据年龄 `<=1500 ms` 为新鲜、`>1500 ms` 为过期。`ProbeLink::poll(uint64_t nowUs)` 不长时间阻塞；`bool ProbeLink::copyLatest(ThermalFrame&, FrameMeta&) const` 只返回完整快照。配置含轮位、串口/DE 引脚及已确认运行参数。

- [ ] 先用假时钟测试未接探头可继续启动、请求超时不重入、CRC 错不刷新有效时间、1500/1501 ms 边界、恢复后等待有效帧、退避重试有界。测试失败后实现调度核心并复跑 `-Case schedule`。

  ```cpp
  assert(isFresh(1500000, 0));
  assert(!isFresh(1501000, 0));
  ```

- [ ] 从实际台架板型号取得可用引脚，检查启动配置脚、USB、SPI2 和 UART 冲突，写入 `notes/pinmap.md`；不要借用旧 v3 引脚。配置缺失须编译时报明确信息，不提供可误接线的猜测默认值。
- [ ] 编写完整 `.ino` 与 ProbeLink，启动即报告版本、轮位、配置、供电状态与初始化结果；记录 CRC/长度/超时/恢复计数并节流；异常不无限等待。配置采用版本与 CRC、双记录/可恢复策略，只在显式修改时写入，启动不得自动写探头设置。
- [ ] 新编译脚本按 `-Target channel|display` 选择 FQBN，通过 `--libraries firmware/libraries` 加入共享库；使用独立临时构建目录，不改写源文件。运行 `powershell -File scripts/compile_wired_tire.ps1 -Target channel`，成功以 CLI 退出码 0 和完整日志为准。
- [ ] 实物断开/接回探头、拔掉 USB-RS485 输入、重启单元；确认启动和恢复日志及温度有效时间正确。做配置保存中断电恢复实验，记录结果并提交 `firmware: 添加独立RS485胎温采集固件`。此任务不启用尚未验证的生产 BLE 字段策略。

## Task 5: BLE 字节编码与单轮 RaceChrono（关卡 B）

**Files:** Create `src/RaceChronoPacket.h/.cpp`、`tests/wired_tire/test_racechrono.cpp`、`firmware/wired_tire_channel/RaceChronoBle.h/.cpp`、`notes/validation_ble.md`；Modify channel `.ino`、README 和接口文档。

**Interfaces:** 定义 `LegacyAuxFields` 保存经验证的非温度字段原始字节；`bool buildRaceChronoPacket(uint8_t characteristic, const ZoneResult&, const LegacyAuxFields&, uint8_t out[20])` 编码一包。`bool canNotify(const FrameMeta&, const ZoneResult&, uint64_t nowUs, bool connected)` 为发送门控。BLE 运行层只使用已通过门控的编码结果。

- [ ] 先写字节断言：B0=`0x02`；八温度占 B4–B19、小端，-100 计数编码 `9C FF`，0 保持 `00 00`；任何分区无效或帧过期不生成正常发送；所有输出长度恰好 20。运行 `-Case racechrono` 取得失败，再实现编码与门控并复跑。

  ```cpp
  assert(packet[0] == 0x02 && packet[4] == 0x9C && packet[5] == 0xFF);
  ```

  此例在首分区计数为 -100、八区全部有效时构建一包后断言，不将原始字节与摄氏度混用。
- [ ] 以现有服务、三个特征及名称格式建立单轮设备，后缀来自稳定的唯一标识。启动广播、连接、断开及重广播有日志，回调不放阻塞式长 delay；连接后启动等待参考现有 800 ms。
- [ ] 单轮实测无高度/无电池时的保留字段策略，记录实际字节与手机显示；测试断线、重新连接、传感器拔出后手机保留最后值的行为。此处不能预设 `0xFFFF` 等未经文档支持的无效码，也不能假装测得电量/高度。
- [ ] 冻结经验证的 `LegacyAuxFields` 配置及三特征完整字节表、单位、缩放、轮位、名称、通知间隔和失效行为到接口文档。若不能满足现有 RaceChrono 显示要求，保持关卡未通过并报告具体问题。
- [ ] 重跑核心测试与 channel 编译，提交 `firmware: 接入单轮RaceChrono热图输出`。BLE 有效不等于新采样，约 30 ms 通知调度不能写成约 33 Hz 测温。

## Task 6: 四轮 BLE 与集中安装验证（关卡 C）

**Files:** Modify `notes/validation_ble.md`、channel README、接口文档；Create `notes/validation_matrix.md`。

**Interfaces:** 消费任务 5 的同一固件，以四份实际配置实例运行。产生手机型号、OS、RaceChrono 版本、四个唯一名称/地址/轮位、固件版本和安装布置对应表。

- [ ] 编译并配置 FL/FR/RL/RR，四个真实探头各走独立 RS485；配置与实物标签逐一核对，不用同一个探头数据模拟四轮。
- [ ] 将四个无线单元按拟用屏幕/外壳遮挡条件布置，RaceChrono 添加四个设备；分别移动每轮热源，验证只改变相应轮位的八分区。
- [ ] 连续运行至少 60 min，记录掉线、重连、错误计数及供电；逐个断开探头、重启单元并验证其余三轮继续工作。
- [ ] 记录关卡 C 的 PASS/FAIL 与证据路径，提交 `docs: 记录四轮胎温BLE共存验证`。未通过时不冻结正式 PCB 天线布局，不改成一个 DIY CAN 设备绕开需求。

## Task 7: 板内 SPI 快照协议与原生验证

**Files:** Create `src/SnapshotWire.h/.cpp`、`tests/wired_tire/test_snapshot.cpp`；Modify接口文档。

**Interfaces:** 提供 `EncodeResult encodeSnapshot(const ThermalFrame&, const FrameMeta&, const ZoneResult&, const LinkStats&, uint32_t ageMs, uint32_t requestToken, uint8_t*, size_t)`、`DecodeResult decodeSnapshot(const uint8_t*, size_t, WheelId expected, uint32_t expectedToken, Snapshot&)`、`uint32_t wireCrc32(const uint8_t*, size_t)`。`Snapshot` 聚合已定义类型及数据年龄、请求 token；`LinkStats` 为 CRC/长度/超时/恢复/丢弃计数。CRC32 采用 IEEE 标准，测试向量 `123456789 -> 0xCBF43926`。

- [ ] 定义固定 1728 字节线协议：64 字节头、1536 字节像素、96 字节有效位图、16 字节八分区、4 字节 CRC32、12 字节零填充。头部偏移：0–3 魔数 `WTH1`；4 版本=1；5 轮位；6 状态；7 分区有效位；8 启动 ID；12 接收序号；16 接收时间 u64；24 年龄 ms u32；28 载荷长度 u16=1648；30 ROI 起始行；31 状态标志（bit0 镜像、bit1 BLE 连接、bit2 BLE 启用）；32 配置版本 u32；36/40/44/48/52 五项错误计数 u32；56 请求 token u32；60–63 保留零。多字节字段均小端，CRC 覆盖前 1712 字节，填充不承载业务字段。
- [ ] 先测试缺失/重复数据、CRC 错误、错误轮位、未知版本、错误长度、符号温度、序号回绕、启动 ID 改变，以及同一内容但年龄增大。断言失败不替换旧快照，序号不是采样次数。运行 `-Case snapshot` 取得失败。

  ```cpp
  assert(wireCrc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xCBF43926);
  ```

- [ ] 实现编码/解码，检查长度、范围与保留字段，不对缓冲执行未对齐结构体强转。帧状态与 `FrameState` 共用定义，重启后允许接收较小序号但先重新建立有效状态。
- [ ] 重跑全部核心测试，将字段与 CRC 算法写入接口文档，提交 `firmware: 添加中控SPI热图快照协议`。

## Task 8: SPI 主从联调与四轮显示模型

**Files:** Create channel `SnapshotSlave.h/.cpp`；Create display `.ino`、`DisplayConfig.h`、`SnapshotMaster.h/.cpp`、README；Create `src/ThermalView.h/.cpp`、`tests/wired_tire/test_view.cpp`、`notes/validation_spi.md`；Modify channel `.ino`、pinmap。

**Interfaces:** `SnapshotSlave::poll()` 排队完整事务；`bool SnapshotMaster::pollWheel(WheelId, SnapshotDelivery&)` 推进非阻塞请求状态机；`SnapshotDelivery` 包含 `Snapshot snapshot`、主机 `receivedLocalUs`、主机测得的 `requestElapsedMs`。`ViewState updateView(const SnapshotDelivery&, uint64_t nowLocalUs)` 输出 `bool fresh` 及总数据年龄，不将主从 MCU 绝对时钟相减。总年龄保守取从机准备响应时年龄＋主机请求到接收耗时＋接收后本地经过时间，使用饱和运算防止溢出。通道离线时返回失败，不等待所有轮位。

- [ ] 用原生假时钟测试：从机报告数据年龄 1400 ms，请求耗时为零时主机接收后再过 101 ms 就失效；请求本身耗时 101 ms 时收到即失效；反复重读同一陈旧快照不能续命；四轮独立、主从时钟相差数小时仍正确。运行 `-Case view` 先失败再实现通过。

  ```cpp
  assert(!updateView(delivery, delivery.receivedLocalUs + 101000).fresh);
  ```

  此例 `delivery` 的从机数据年龄为 1400 ms，请求耗时为零。另测带有效 CRC 但 token 不是本次请求的旧排队响应被拒绝。

- [ ] 以 1 MHz、mode 0 作为板内 SPI 原型起点，四个独立 CS。执行修订：每次事务均为 1728 字节全双工，请求首 8 字节为命令 `0x52`、轮位、u32 token、CRC16，其余零填充；从机返回此前排队的快照，主机重复同一 token 直到收到匹配响应。固定长度避免短请求与长响应的排队阶段混淆，代价是至少两次事务及额外带宽；这一取舍须实测。单轮请求总期限初始 100 ms，超时不复用在途 DMA 缓冲，其他轮继续；`pollWheel` 每次仅推进状态而不等待。SPI 从机 DMA 缓冲对齐和长度满足所选 C3 驱动，GPIO 与其 UART/USB 不冲突。
- [ ] 使用双缓冲或带生命周期的快照池：事务在途期间不覆写正在发送的数据。重新读取时年龄从最后有效探头响应计算，不从最后一次 SPI 传输重置。
- [ ] 在真实 C3/P4 上分别执行从机复位、P4 复位、断开一个 CS/握手、从机不就绪实验；原生并发模拟需逐字节验证不出现新旧像素混合，实物记录错误计数和 BLE 是否继续工作。
- [ ] 运行 `compile_wired_tire.ps1 -Target channel` 和 `-Target display`，保存原型引脚及实测记录，提交 `firmware: 接入四路SPI快照采集`。

## Task 9: 7 英寸面板确定与完整热图界面

**Files:** Create display `PanelDriver.h/.cpp`、`HeatmapRenderer.h/.cpp`、`notes/screen_selection.md`、`notes/validation_display.md`、`tests/wired_tire/test_heatmap.cpp`；Modify `ThermalView.h/.cpp`、display `.ino`/README/配置、pinmap。

**Interfaces:** `bool PanelDriver::begin(const PanelConfig&)`、`bool PanelDriver::present(const uint16_t* rgb565, size_t count)`。纯函数 `void renderHeatmap(const ThermalFrame&, const ColorScale&, PixelSurface&)` 定义在共享库 `ThermalView.h/.cpp`，显示侧 `HeatmapRenderer` 负责调用它与硬件提交，不将硬件头文件引入原生测试。`ColorScale` 全屏共享；`PixelSurface` 显式指定宽/高/stride/容量。温度输入只读。

- [ ] 确定实际 7 英寸 IPS 面板、1024×600 目标、驱动 IC、DSI 初始化、背光、工作温度、FPC 针脚与机械尺寸；取得数据手册后才生成 `PanelConfig`。日光可读性由实际安装测试决定，不以分辨率代替亮度验证。
- [ ] 先测试四区边界、同温同色、负温/0°C/范围外/无效像素、最大值排除无效点、输出缓冲哨兵无越界、全帧 768 点都有对应显示位置，运行 `-Case heatmap` 取得失败再实现通过。

  ```cpp
  assert(pixels.front() == guard && pixels.back() == guard);
  ```

  此例为输出画布前后放置哨兵，仅将中间有效区域传入 `PixelSurface`，绘制后检查未越界；颜色断言比较相同 `ColorScale` 下的两轮相同温度像素。
- [ ] 用 `esp_lcd` 和选定面板驱动实现 RGB565 双缓冲与同步提交；核对 P4 芯片修订、PSRAM、驱动对齐要求和 MIPI DPHY 专用供电。未完成真实面板原型不冻结正式屏幕连接器。
- [ ] 布局固定 FL/FR/RL/RR 四区，显示轮位、全热图、有效最高温度、采集和 BLE 状态；过期灰显并带数据年龄。共享色标可配置，绘制不使用时间插值伪造新测温帧，避免阻塞 SPI 和采集。
- [ ] 在四路真实采集及 BLE 同时运行时记录至少 10 min 的绘制 FPS、提交间隔、最大卡顿与端到端热源延迟，目标 30 FPS；结果不足时记录瓶颈并优化，不填写推算 FPS。重跑两目标编译及核心测试后提交 `firmware: 添加七英寸四轮完整热图界面`。

## Task 10: 电源、原理图、PCB 与线束（投板关卡）

**Files:** Create `hardware/wired_tire_central/README.md`、`notes/power_budget.md`、`notes/harness.md`、`notes/design_review.md`、`notes/manufacturing_manifest.md`；建立 `schematic/`、`pcb/`、`bom/`、`gerber/`、`3d/` 的正式设计源文件及输出；Modify pinmap、screen_selection。

**Interfaces:** 消费已通过 A–C 的记录、任务 8 的 SPI/引脚证据和任务 9 的真实面板原型，产生统一网名、连接器针脚表、物料版本和制造清单。当前仓库未发现可编辑 EDA 源工程，实施前需确定用户能够维护的 EDA 格式和可用工具；该信息缺失时可准备电路分块、物料计算与针脚表，但不能声称已完成可制造 PCB。无可编辑源文件不能只交付截图声称完成。

- [ ] 测量屏幕背光、四个 C3 广播/连接及四探头工作时的电流，计算稳压器、启动电流、线束压降、热耗散与裕量；记录点火取电点的启动压降和断电时序。
- [ ] 设计入口保险/限流、反接与瞬态保护、逻辑/屏幕电源、四路独立探头保护，以及 P4 面板所需专用电源；受控验证单支路保护时避免以车辆电池直接短接线束。
- [ ] 固化五个 MCU 的维护/烧录通道、启动电平、全部 GPIO、SPI 握手、RS485 DE/RE、终端与参考地、接口针脚、天线净空和屏幕机械固定；修改任何原型引脚均记录新旧对照。
- [ ] 完成原理图/ERC、电源和启动时序审查后布板；PCB/DRC 检查天线、DSI 差分走线、回流、电源、ESD 路径、热和振动固定。将规则、数值及检查结果保存，不能只写“已检查”。
- [ ] 对照 A–C、SPI 和屏幕证据核查投板条件，生成 BOM/CPL、Gerber/钻孔、原理图 PDF 和版本清单并交叉核对。提交 `hardware: 添加四路有线胎温中控原型设计`。下单、付款或发送制造文件须另按用户授权执行，计划批准不等于采购授权。

## Task 11: 集成板带电、故障恢复与实车验证

**Files:** Create `notes/validation_power.md`、`notes/validation_vehicle.md`；Modify `notes/validation_matrix.md`、全部验证记录和两个固件 README；必要修改只落在新版本驱动/配置内。

**Interfaces:** 消费制造清单和固件版本，产生每块板的序列标识、测点电压、接线版本、运行日志与 PASS/FAIL 结果。每一项结果关联实际仪器或手机版本。

- [ ] 限流台架上电前测短路及关键电源阻值，分级检查入口、5 V/3.3 V/面板专用电源、各 MCU 启动与烧录，再逐路接探头；记录实际测点与波形。
- [ ] 使用前轮 2 m、后轮 4.5 m 完整线束重复 A–D，四路采集/显示/BLE 同时运行至少 60 min；记录帧率、延迟、错误、掉线和温升。
- [ ] 在受控台架逐个断开探头、造成限流支路故障、重启采集单元、停止显示主控、连接/断开手机；断言其他轮继续工作，失效分区有提示且不正常发送 0°C。
- [ ] 做至少 30 次点火开关循环、启动压降回放和显式配置保存中断电测试；保存波形/日志，验证能从有效配置启动，无随机 BLE/传感器卡死。电源瞬态等级和仪器测试条件必须在 power_budget 中先定义，不能用普通开关实验代替汽车浪涌验证。
- [ ] 装车检查日光可读、转向/悬挂间隙、线束拉力、镜头视场、点火/熄火动作及真实用电干扰。若数据冻结、仅 USB 正常或频繁掉线，依 AGENTS.md 先定位电源/接地/线束，不通过平滑或重启掩盖。
- [ ] 将不通过项修复后只重跑受影响的验证与必要回归，提交 `docs: 记录有线胎温中控集成验证`。赛道/道路操作由有条件且获授权的人执行；无实物条件时准确停留在该关卡，不把软件测试标成车辆测试通过。

## Task 12: 构建、完整交付与文档同步

**Files:** Create `scripts/package_wired_tire.ps1`；Modify `scripts/check_docs_consistency.py`、`.github/workflows/project-checks.yml`、README、CODEX、`docs/02_硬件版本说明.md`、`docs/03_固件结构与命名.md`、`docs/06_测试与交付检查清单.md`、`docs/11_四路有线胎温中控接口.md`、`firmware/README.md` 及新硬件/固件 README。

**Interfaces:** 打包脚本输出两个完整 Arduino sketch 目录、共享库 ZIP、精确依赖版本/安装说明、轮位配置表、接口文档和验证摘要。构建脚本须能在干净临时目录使用这些交付文件完成两个目标编译。

- [ ] Arduino IDE 通过库 ZIP 安装共享库，交付 `.ino` 保留所有 setup/loop/include；锁定 core、面板驱动版本和校验信息。硬件原型与稳定交付状态按证据分别标注。
- [ ] 调整 CI：旧任务不得再把新增 P4 sketch 通过 `-All -Fqbn ...esp32c3` 编译；旧固件显式列出目标，新固件使用 channel/display 独立矩阵，保留原有检查。CI 使用相同 core 版本；原生协议测试不能只依赖固件编译。
- [ ] 新增文档一致性断言：新方向独立版本、无高度、四轮身份、非 DIY CAN、SPI 版本与针脚表来源一致；旧 v3 和三种刹车版本保护断言继续保留。
- [ ] 运行全部核心测试、Python 工具测试、两目标编译、文档一致性检查；本任务生成了原理图 PDF 时运行 PDF 元数据检查。运行交付包解压后的独立编译，确认未依赖仓库外未记录文件。
- [ ] 执行 `git diff --check`，检查只包含计划内新版本和必要构建/文档变更；保留硬件未验证的实际状态，不把未完成项写成 PASS。提交 `docs: 完善有线胎温中控交付与构建说明`，提供最终文件路径、构建结果、硬件验证范围与剩余限制。

## 里程碑与交接

| 里程碑 | 对应任务 | 没有硬件时仍可完成的部分 |
| --- | --- | --- |
| 协议和算法可测试 | 1–3 | 类型、解析器、算法、抓包工具及合成样本测试；关卡 A 只能等待真实数据 |
| RaceChrono 四轮验证 | 4–6 | 构建与编码测试；关卡 B/C 需要探头、四个无线单元和目标手机 |
| 完整热图显示原型 | 7–9 | 快照协议与显示模型测试；真实 SPI、面板帧率和可读性不能离线认证 |
| 定制中控板 | 10–11 | 有证据后才能固化设计；制造、焊接、测量和装车不能由文档替代 |
| 可复现交付 | 12 | 软件打包和编译；硬件交付状态必须随真实记录报告 |

采用原生执行时按顺序实施，每个任务完成并核对证据后再提交；采用子代理执行时同样受依赖和硬件关卡约束，不并发修改共享接口，不得用多个代理推测不存在的实测结果。

本计划写入后先由用户审阅并选择执行方式，再进入执行技能。本轮只提交计划及审阅状态同步，不创建上述产品代码、测试代码或硬件文件。

## 官方接口资料

- [Arduino-ESP32 支持目标](https://docs.espressif.com/projects/arduino-esp32/en/latest/getting_started.html)：支持 P4 不代表任意面板初始化都可直接使用。
- [Espressif MIPI-DSI 驱动说明](https://docs.espressif.com/projects/esp-idf/en/stable/esp32p4/api-reference/peripherals/lcd/dsi_lcd.html)：显示缓冲对齐和 DPHY 专用供电应按最终驱动版本核对。
- [ESP32-C3 数据手册](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_en.pdf)：核对 UART、SPI2、DMA、GPIO 和启动配置限制。
