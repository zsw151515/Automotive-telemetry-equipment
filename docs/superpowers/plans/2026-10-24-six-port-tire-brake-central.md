# 六接口胎温与前刹车中控 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为同壳安装的微雪 7B 与六接口底板实现四轮完整胎温、双前刹车独立采集/报警/BLE、触屏切页与可确认保存的设置，并保留真实硬件放行条件。

**Architecture:** 四个胎温 C3 保留既有协议；第五 C3 管理两刹车总线、共用热模型、蜂鸣、NVS 和 N BrakeTemp BLE。P4 通过同壳短距离 SPI 统一调度五从机，四轮使用原 Snapshot v1，刹车采用独立 BrakeLink v1；触屏只提交命令和显示控制器实际状态。

**Tech Stack:** C++17、Arduino CLI 1.5.0、现有 Arduino-ESP32 3.3.7、纯 C++ 核心与原生测试、Python 工具；P4 使用固定版本 EK79007/GT911 底层驱动和现有 RGB565 绘制。仅当驱动/实际芯片修订不兼容证据成立时，将 P4 隔离到固定 core 3.3.11，不改变 C3 或旧版本环境。

**Spec:** `docs/superpowers/specs/2026-10-24-six-port-tire-brake-7b-design.md`。用户在核对 7B 中文资料后要求继续，据该设计编写本计划；执行方式沿用原生执行，本文已获用户“是请继续”确认，执行离线部分；不等于固件已获实物验证。

## Global Constraints

- 同一个外壳内为 7B 完整套件＋六口底板；四胎温 FL/FR/RL/RR＋两前刹车 FL/FR，无高度、无轮端电池，点火供电。
- 本地胎温页四幅完整 32×24；胎温 RejsaRubberTrac 四身份、0x1FF7、三特征各20字节，B4–B19 为八个小端 int16 温度计数/10。
- 刹车为独立 N BrakeTemp、0x1FF8、主特征0001/filter0002；0x100–0x107 的实际布局/计算完整更新文档，CAN ID 小端、温度 payload 大端，0x7FFF 温度无效。
- 保留两路有效盘温最大值驱动的一套 V1.6 模型/ALM；默认600°C、100–900°C、步进10°C；ALARM 不能被普通提示音开关屏蔽，临时静音60秒，FAIL状态本身静音。
- 切页/P4重启不得停止刹车 MCU 的采集、模型、蜂鸣、BLE；刹车 MCU 故障本身没有冗余，大屏显示未知而非正常。
- 胎温过期1500ms；刹车2500ms；显示侧控制状态过期1000ms；各 MCU 绝对时钟不相减。所有期限使用单调 uint64 微秒，零时刻有效。
- 两刹车独立 RS485、9600 8N1，只读03/04；每端核准地址，基线01/02。胎温115200 8N2/65不混入同一总线。
- 13个P4板间信号尚未核准。17引出脚包含绑带34/36及VO4域46–48，不能当成17个无条件3.3V GPIO。SPI必须显式赋脚，默认32/33冲突背光/复位。
- 第五C3双UART＋SPI/READY＋蜂鸣约12 GPIO，USB日志另占18/19；UART0启动时DE硬件关闭；所有实际引脚须审查，默认-1且I/O禁用。
- 原v3、MAX31856和独立刹车 `.ino` 不改；新刹车固件完整交付，不依赖复制旧OLED/按键循环。旧0c29469 ZIP及其59项原生/10项工具结果只属于旧包。
- 保留原生执行；不得生成或宣称已验证PCB/Gerber/BOM、实车、手机连接或30FPS。相机、音频、网络及无用LVGL依赖不作为首版功能。

## Review Focus

1. NVS可能已写成而ACK丢失/读回失败：界面不能误称回滚或重复重置模型，覆盖任务4/9。
2. P4重启、触摸粘住或一种探头不响应：后台报警/其他通道仍推进，不能因切页或等待回执停顿，覆盖任务7–10。
3. 零温/负温、零时刻、32位边界和长任务停顿：不能变成假零温或延长静音/过期，覆盖任务2/3/5。
4. CRC正确但会话、配置版本、命令ID/内容不匹配，或缓存淘汰后的旧命令：不能执行设置/延长60秒，覆盖任务4/5/8。
5. 默认SPI脚、VO4电平、绑带与双UART日志冲突：未核准profile必须在外设启用前失败，覆盖任务1/7/10，实际电平仅由硬件关卡关闭。

## 已取得的资料、执行范围和工程选择

资料见 `hardware/wired_tire_central/notes/7b_source_audit.md`，含官方PDF页码、BSP提交d7567970be6d6a94077727eb69d580d63a898eab及示例提交9fcd49a8c927a00f5583c22d568b3448addbe51c。本机P4可选 `ChipVariant=prev3/postv3,PSRAM=enabled`，postv3 SDK最低修订3.01；实际芯片尚未知。板上EK79007、背光32反相5kHz/10bit、屏复位33、GT911 I²C7/8；触摸RST/INT原理图与BSP差异待核对。

本计划先实现可离线验证部分，硬件行保持未完成。允许的新移植健壮性修正明确如下：异常Modbus帧也验CRC；64位时钟消除旧millis静音回绕问题；模型dt≤0不积分、dt>2s重新初始化且重新计连续确认时间，不用旧0.1s替代停顿；allow-all filter间隔按请求且不快于150ms。参数和正常采样模型不因此变更，旧源码保留。上述选择作为计划的一部分供用户审阅。

## 文件结构与契约

共享库仍为 `firmware/libraries/WiredTireCore/src`；新刹车文件使用 `wired_brake` 命名空间，避免将刹车当第五个胎温 WheelId。`BrakeTypes.h` 定义 `BrakeConfig{uint16_t alarmC=600;bool warningBeep=true;}`、`BrakeReading{int16_t temp10;bool valid;uint64_t receivedUs;}`、`BrakeState{Ok=0,Warn=1,Alarm=2,Fail=3}`、`BrakeModelResult{int16_t max10,fast10,core10;uint16_t dose10;bool anyFresh,modelValid;uint8_t failMask;BrakeState state;}`、`BrakeTelemetry`（以下线协议的命名字段）及 `BrakeAck`（result为按线表定义的`AckResult`枚举）。`CommandOp{Read,ApplySave,Mute60}`与`BrakeCommand{op,hostBootId,targetBrakeBootId,commandId,expectedConfigVersion,candidate,muteMs}`按线表类型定义；op无关候选字段不参与语义，并在编码时置零。`ControlIntent{CommandOp op;BrakeConfig candidate;}`只表达用户意图，boot/ID/版本由客户端补齐。结果初始温度0x7FFF、state=Fail、有效位false、age=UINT32_MAX、boot/token/序号0，不能因零初始化误报正常。

| 文件组 | 责任 |
| --- | --- |
| `BrakeTypes.h`、`BrakeModbus.h/.cpp` | 数据类型、单寄存器帧、候选/接收调度 |
| `BrakeModel.h/.cpp`、`BrakeBuzzer.h/.cpp` | 纯模型、锁存/状态、非阻塞蜂鸣与静音 |
| `BrakeSettings.h/.cpp`、`BrakeWire.h/.cpp` | 配置存储抽象、命令去重与回执、固定线协议与年龄 |
| `BrakeCan.h/.cpp` | 八通道编码、过滤器/发送时序 |
| `firmware/wired_brake_channel/` | 完整新sketch、双UART、NVS、BLE、SPI从机适配 |
| `BoardSpiBus.h/.cpp`、`BrakeLinkMaster.h/.cpp` | 显示目录内五peer统一总线及刹车控制客户端 |
| `TelemetryUi.h/.cpp`、`RasterPrimitives.h/.cpp` | 共享库内纯绘制/触摸状态机；旧热图API保留 |
| `Board7BProfile.h`、`Board7BPanel.h/.cpp`、`Board7BTouch.h/.cpp` | 显示目录内7B板级实现与核准门槛 |

### BrakeLink v1（任务5冻结并验证）

请求与响应各固定256字节、全双工流水；首个旧token响应丢弃，与原1728字节胎温从机各有独立CS。全部多字节字段小端，248–251为前248字节IEEE CRC32，252–255零填充，未定义字节必须零。

| 方向/偏移 | 字段 |
| --- | --- |
| 请求0/4/5/6–7 | BRC1 / version1 / op：0 Read、1 ApplySave、2 Mute60 / 保留零 |
| 请求8/12/16/20/24 | u32 token / hostBootId / targetBrakeBootId / commandId / expectedConfigVersion |
| 请求28/30/31/32 | u16 alarmC / u8 warningBeep / 保留零 / u32 muteMs |
| 请求36–247 | 零；Read的命令/候选/静音字段为零，可用targetBoot=0发现节点；变更必须匹配非零目标boot、非零命令ID，Mute只允许60000ms |
| 响应0/4/5/6/7 | BRK1 / version1 / status类型固定1 / BrakeState / flags |
| 响应8/12/16/20/24 | u32 brakeBootId / sequence / token / stateAgeMs / u64 sourceStateUs（仅诊断） |
| 响应32/36/40/42/43 | u32 configVersion / savedVersion / u16 alarmC / warningBeep / 两路validMask |
| 响应44/46/48/50/52/54 | int16 FL/FR/max/fast/core 温度计数 / u16 dose10 |
| 响应56/60/64 | u32 FLageMs / FRageMs / muteRemainingMs |
| 响应68/72/76/80 | u32 CRC/长度/超时/恢复计数 |
| 响应84/85/86–87 | u8 ackResult / ackOp / 保留零 |
| 响应88/92/96/100/104/105 | u32 ackHostBoot / ackCommandId / ackTargetBoot / ackConfigVersion / u8 configOrigin / enabledMask |
| 响应106–247 | 零 |

flags bit0 BLE启用、bit1连接、bit2有效配置、bit3模型有效、bit4配置已保存、bit5配置恢复/需检查，其余零；validMask必须是enabledMask子集。无有效探头时max使用0x7FFF，模型无效时fast/core使用0x7FFF；原始每路可保留最后值但须结合valid/age判读。configOrigin=0默认/1新配置/2旧配置迁移。ackResult=0 None/1 Pending/2 AppliedSaved/3 Muted/4 Invalid/5 StaleBoot/6 VersionConflict/7 IdConflict/8 StorageFailed/9 OutcomeUnknown/10 SessionMismatch/11 Busy。CRC/语义错误不覆盖旧状态，不执行命令。

### Task 1: 资源/版本台账及硬件门槛

**Files:** Create `notes/7b_resource_review.md`、`notes/third_party_manifest.md`；Modify `notes/pinmap.md`、`notes/screen_selection.md`、spec状态。所有notes位于`hardware/wired_tire_central/notes/`。
**Interfaces:** 产生资料版本/许可/哈希、`DisplayCoreVersion`、`ChipVariant`选择规则和板资源审查记录。该记录对应任务10的`electricalApproved/strapApproved/vo4Approved`条件，目前均未实物核准，供任务7/10消费；不产生已核准接线。
- [ ] 对照固定BSP/原理图记录P1/P3脚位、34/36绑带、VO4域、背光32/复位33及触摸差异；列出13信号和第五C3的GPIO预算、日志USB和DE上电默认关闭措施。
- [ ] 锁定底层EK79007与GT911使用的具体源文件、版本、许可证和SHA256，避免直接拉整套LVGL/音频/网络BSP。现有3.3.7为首选，3.3.11仅作为有证据时的隔离显示构建分支。
- [ ] 核对本机boards.txt与sdkconfig，记录C3 `CDCOnBoot=cdc`、P4 prev3/postv3及PSRAM开启；不得安装或更新全局旧core作为资料核对手段。
- [ ] 运行 `git diff --check`。Expected: 无错误；所有未测电平/芯片/实际板型标为待核准，硬件profile保持禁用。提交 `docs: 核对六口中控板级资源与依赖`。

### Task 2: 双路刹车数据与只读Modbus核心

**Files:** Create `BrakeTypes.h`、`BrakeModbus.h/.cpp`、`tests/wired_tire/test_brake_modbus.cpp`、`scripts/capture_brake_rs485.py`、`tests/wired_tire/test_capture_brake_rs485.py`；Modify `WiredTireCore.h`、`scripts/test_wired_tire_core.ps1`。共享新文件均位于上表库src目录；runner新增`-WorkspacePath`，新构建产物落本计划`.superpowers/sdd/2026-10-24-six-port-tire-brake-central/`，编译器仍可用明确指定的既有Zig路径，不覆写旧计划日志/二进制。
**Interfaces:** `bool decodeBrakeRegister(const uint8_t*,size_t,uint8_t address,uint8_t function,int16_t& temp10)`；`BrakePoller::poll(uint64_t nowUs)`返回`PollAction{None,Request,Timeout}`；`bool response(uint64_t nowUs,bool accepted)`；每端独立实例。
- [ ] 先写 `zero_negative_and_limits`（0、-400、12000合法，-401/12001拒绝）、`bad_address_function_crc_exception_truncation_preserves_output`（异常帧也验CRC）、`late_tail_never_renews_age`；验证0x03/04 byteCount严格2，候选03/0000→04/0000→03/0001→04/0001。同时写Python抓包测试：已知请求向量、拒绝广播/写指令、超时/断开关闭串口、输出不覆写。
- [ ] Run `powershell -File scripts/test_wired_tire_core.ps1 -Case brake_modbus` 和 `python -m unittest discover -s tests/wired_tire -p test_capture_brake_rs485.py`。Expected: 新API/脚本不存在或断言FAIL，而非缺编译器/依赖；尚不接真实串口。

```cpp
std::vector<uint8_t> frame{1,3,2,0,0,0,0}; repairFixtureCrc(frame);
int16_t value = 999;
CHECK(decodeBrakeRegister(frame.data(),frame.size(),1,3,value) && value == 0);
frame[0] = 2; repairFixtureCrc(frame);
CHECK(!decodeBrakeRegister(frame.data(),frame.size(),1,3,value) && value == 0);
```
- [ ] 实现有界接收、请求20ms期限、每100ms轮询一路、2500ms有效期；读FIFO前处理超时，新请求前丢弃旧尾字节，候选只在无在途请求时推进。不发送06/10配置写入。新增只读抓包CLI，显式提供port/address/function(03或04)/register(0或1)/baud/stopbits/seconds/新output目录；pyserial懒加载，有限读写超时，原始收发/单调时间和metadata保留，hardware_validated恒false，不复用只发65的胎温抓包请求。
- [ ] 同两条命令重跑。Expected: C++及Python全部用例PASS，两端失败/恢复独立且不能把未请求数据提交为新温度。
- [ ] 提交 `firmware: 添加双路刹车只读采集核心`。

### Task 3: 纯热模型、状态与蜂鸣

**Files:** Create `BrakeModel.h/.cpp`、`BrakeBuzzer.h/.cpp`、`test_brake_model.cpp`、`test_brake_buzzer.cpp`（tests均在`tests/wired_tire/`）；扩展runner。
**Interfaces:** `BrakeModelResult BrakeModel::tick(const std::array<BrakeReading,2>&,uint8_t enabledMask,const BrakeConfig&,uint64_t nowUs)`、`reset(uint64_t)`及`const BrakeModelResult& result() const`；`BrakeBuzzer::begin(uint64_t bootUs)`、`mute60(uint64_t)`及`bool output(BrakeState,bool warningEnabled,uint64_t nowUs)`，不依赖Arduino时钟。版本、保存和BLE字段由任务7在模型结果之外组装，模型不持有配置副本。
- [ ] 先写 `zero_time_core_hold_6s`：每100ms输入610°C，t=0有效，6s前不ALARM、6000000us为ALARM；610避免误触发ALM+40的5s确认支路。另测650°C连续输入在5s触发辅助确认；加`alarm_beats_partial_fail`、`all_fail_resets_model`、`same_alarm_does_not_reset`、`clock_boundary_long_gap_reinitializes`。
- [ ] 写蜂鸣 `startup_3s_and_warn_alarm_patterns`、`mute_expires_at_60000000us_without_millis_wrap`、`warning_off_does_not_disable_alarm`、`fail_is_silent`，跨32位毫秒边界仍正确。
- [ ] 分别Run runner `-Case brake_model`、`-Case brake_buzzer`。Expected: 新类型/函数缺失导致FAIL。

```cpp
BrakeModel model; std::array<BrakeReading,2> readings{};
for (uint64_t t=0;t<=6000000;t+=100000) {
  readings[0]=readings[1]=BrakeReading{6100,true,t};
  CHECK((model.tick(readings,3,BrakeConfig{},t).state == BrakeState::Alarm) == (t==6000000));
}
```
- [ ] 实现τ fast0.8s/core升20s降45s，滤波系数dt/(τ+dt)；Dose起点max(ALM−80,80)，强度clamp((core−起点)/80,0,2.5)，以dt积分，冷却0.7/s、范围0..99。Core≥ALM持续6s；core≥ALM−20且(Dose≥20或fast≥ALM+40)连续5s亦锁存；释放三条件core≤ALM−40、Dose≤8、fast<ALM同时满足。WARN为core≥ALM−30或Dose≥10或fast≥ALM，状态优先ALARM→WARN→FAIL→OK。蜂鸣ALARM150/300ms、WARN80/1500ms，启动提示非阻塞。
- [ ] 重跑两case。Expected: 全PASS，NaN/inf或异常时间不产生有效正常值。提交 `firmware: 移植刹车热模型与独立蜂鸣状态机`。

### Task 4: 唯一配置源、掉电存储与幂等命令

**Files:** Create `BrakeSettings.h/.cpp`、`test_brake_settings.cpp`、`tests/wired_tire/brake_test_support.h`；扩展runner。后者定义`CountingStore`（实现上述存储边界，commitCount统计，可注入各StoreResult）。
**Interfaces:** `ConfigStore::load(PersistRecord&)->LoadResult{Valid,Missing,Invalid,Unavailable}`、`commit(const PersistRecord&)->StoreResult{Committed,NotCommitted,Unknown}`、`readBack(uint32_t version,PersistRecord&)->LoadResult`为硬件抽象；`BrakeControl(ConfigStore&,BrakeModel&,BrakeBuzzer&)`只存引用，`begin(uint32_t brakeBoot,uint64_t nowUs)`才加载配置并初始化；`handle(const BrakeCommand&,uint64_t nowUs)->BrakeAck`、`poll(uint64_t)`、`const BrakeConfig& config()`、`uint32_t configVersion()`。构造函数禁止触碰NVS/外设，调用begin前必须完成硬件门槛。
- [ ] 先测 `save_then_apply_once`、`lost_ack_retry_no_extra_flash_or_reset`、`mute_retry_does_not_extend`、`wrong_boot_session_version_or_same_id_different_payload_no_side_effect`、`torn_record_loads_only_complete_old_or_new`、`unknown_commit_is_not_claimed_rollback`；断电点枚举写前/中/后/ACK前。
- [ ] Run `-Case brake_settings`。Expected: 新API缺失FAIL。

```cpp
CountingStore store; BrakeModel model; BrakeBuzzer buzzer;
BrakeControl ctl(store,model,buzzer); ctl.begin(11,0);
BrakeCommand read{}; read.op=CommandOp::Read; read.hostBootId=7; ctl.handle(read,0);
BrakeCommand cmd{}; cmd.op=CommandOp::ApplySave; cmd.hostBootId=7;
cmd.targetBrakeBootId=11; cmd.commandId=1; cmd.candidate={650,true};
ctl.handle(cmd,1); for(uint64_t t=2;t<=8;++t) ctl.poll(t);
CHECK(ctl.configVersion()==1 && store.commitCount==1);
CHECK(ctl.handle(cmd,9).result==AckResult::AppliedSaved);
ctl.poll(10); CHECK(store.commitCount==1);
```
- [ ] 配置记录固定32字节：BCF1/版本1/提示音/u16 ALM/u32配置版本/保留零，28处CRC32；双slot选择有效最高版本，等版本不同内容报恢复异常。读旧brake12v只作经校验迁移来源，不清旧键、不自动写新值；异常旧配置提示确认。
- [ ] ApplySave校验100..900且步进10、版本及目标boot；先持久化确认再生效。相同已保存配置不写/不复位；仅ALM实际变化复位一次。存储Unknown保留当前有效配置并回传结果未知，阻止后续配置变更直至读回解决；Read不能抹去未决回执。commit返回后没有尚未执行的后台写入，Unavailable不等于NotCommitted；掉电重启可能加载完整旧或新配置，不宣称一定回滚。
- [ ] 最近8命令结果缓存＋当前会话命令ID高水位，缓存淘汰后旧ID仍拒绝；语义指纹不含传输token。新P4会话先Read，旧会话变更拒绝；ID/版本禁止静默回绕。未决保存期间新变更返回Busy且不入执行队列，客户端读回完成后须用新命令提交，不能后来自动执行被拒命令。Mute60仅首次执行起算且不写NVS。重跑caseExpected PASS，提交 `firmware: 添加刹车设置确认与可恢复存储核心`。

### Task 5: 独立BrakeLink编解码与状态年龄

**Files:** Create `BrakeWire.h/.cpp`、`test_brake_wire.cpp`、`test_brake_view.cpp`；Modify `WiredTireCore.h`、runner及brake_test_support（独立CRC32 fixture修复函数和`validBrakeTelemetry()`：boot=1、两路610°C、ALM600、状态WARN、有效位/源年龄/配置元数据一致）；Create `docs/13_六接口中控板间与刹车接口.md`。
**Interfaces:** `bool encodeBrakeRequest(const BrakeCommand&,uint32_t token,uint8_t*,size_t)`、`bool decodeBrakeRequest(const uint8_t*,size_t,BrakeCommand&,uint32_t& token)`；`bool encodeBrakeStatus(const BrakeTelemetry&,uint32_t token,uint8_t*,size_t)`、`bool decodeBrakeStatus(const uint8_t*,size_t,uint32_t expectedToken,BrakeTelemetry&)`；`BrakeDelivery{BrakeTelemetry state;uint64_t receivedLocalUs;uint32_t requestElapsedMs;}`；`BrakeView updateBrakeView(const BrakeDelivery&,uint64_t nowUs)`返回`stateFresh/stateAgeMs/sensorFresh[2]/sensorAgeMs[2]`。
- [ ] 先测固定偏移/符号/CRC32向量、全部长度/保留位/非法枚举/错误token拒绝且输出不变；`state_age_900_plus_request_101_is_stale`、`sensor_age_2400_plus_host_101_is_invalid`、`repeated_reads_and_peer_reboot_do_not_rejuvenate`。
- [ ] Run `-Case brake_wire`及`-Case brake_view`。Expected: 缺实现FAIL。

```cpp
BrakeDelivery d{}; d.state=validBrakeTelemetry(); d.state.stateAgeMs=900;
d.receivedLocalUs=1000000;
CHECK(updateBrakeView(d,1000000).stateFresh);
d.requestElapsedMs=101;
CHECK(!updateBrakeView(d,1000000).stateFresh);
```
- [ ] 按上表实现256字节请求/响应；复用`wireCrc32`，显式逐字段编码，token/响应boot非零，只有发现用Read允许targetBoot=0。stateAge从模型/状态服务的最后执行计算，不能由SPI成功刷新；每路age分别计算。数据年龄叠加向上取整的请求耗时和本地经过时间并饱和，状态1000ms、每路2500ms分开判断，本地时钟回退一律失效。不能把新token当新测温。
- [ ] 重新运行两case。Expected: PASS；接口文档同时写明4胎温v1不变、BrakeLink独立及所有状态字段，提交 `firmware: 添加刹车状态与设置板间协议`。

### Task 6: 刹车DIY CAN编码与过滤器

**Files:** Create `BrakeCan.h/.cpp`、`test_brake_can.cpp`；扩展runner及接口文档。
**Interfaces:** `size_t encodeBrakeCan(uint32_t pid,const BrakeTelemetry&,uint8_t out[20])`；`BrakeCanFilter::apply(const uint8_t*,size_t)`原子更新，`bool due(uint32_t pid,uint64_t nowUs)`；硬件回调只复制有界命令，不阻塞循环。
- [ ] 先测八PID字节/长度，通知4字节LE ID＋BE payload；负温、无效7FFF、Dose×10而ALM不×10；`deny_all/allow_all/custom_interval/dual_endian_pid/zero_interval/12_capacity/malformed_does_not_mutate`。
- [ ] Run `-Case brake_can`。Expected: 缺实现FAIL。

```cpp
auto state=validBrakeTelemetry(); state.state=BrakeState::Alarm; uint8_t packet[20]{};
CHECK(encodeBrakeCan(0x106,state,packet)==5 && packet[0]==6 && packet[1]==1 && packet[4]==2);
state.validMask=2;
CHECK(encodeBrakeCan(0x100,state,packet)==6 && packet[4]==0x7F && packet[5]==0xFF);
```
- [ ] 保留旧命令00严格1字节、01至少3字节（BE间隔）、02至少7字节（BE间隔＋PID大小端兼容），未知/截断拒绝；原子检查容量，重复PID不占新槽，零间隔150ms。默认150ms，请求更慢间隔受尊重、发送不快于150ms；原单PID过滤语义保留。
- [ ] 重跑Expected PASS；文档逐项列0x100..107、payload、单位、缩放、温度无效值和计算定义，手机可粘贴表达式留实测，提交 `firmware: 添加独立刹车RaceChrono协议核心`。

### Task 7: 第五C3完整固件与驱动级回归

**Files:** Create `firmware/wired_brake_channel/wired_brake_channel.ino`、`BrakeChannelConfig.h`、`DualBrakeProbe.h/.cpp`、`BrakeNvsStore.h/.cpp`、`BrakeBle.h/.cpp`、`BrakeSpiSlave.h/.cpp`、README；Create `test_brake_driver.cpp`及最薄硬件边界stubs；Modify compile/link脚本增加`-Target brake`。
**Interfaces:** `BrakeHardwareConfig`含hardwareEnabled/approved、两端rx/tx/direction/address/enabled、SPI sck/mosi/miso/select/ready和buzzerPin，默认disabled/-1；各驱动`bool begin(const BrakeHardwareConfig&)`。`DualBrakeProbe::poll(uint64_t)`与`readings()`；`BrakeNvsStore`实现任务4的ConfigStore；`BrakeBle::poll(uint64_t,const BrakeTelemetry&)`；`BrakeSpiSlave::poll(uint64_t)`、`bool takeCommand(BrakeCommand&)`、`publish(const BrakeTelemetry&)`。完整loop处理期限/采集、模型、收到的控制请求/存储读回，再从最新模型与配置组装状态、输出蜂鸣、推进BLE及发布SPI；任何步骤有预算。
- [ ] 先写真实驱动调用顺序回归：末尾UART字节超期拒绝、一路持续噪声另一条继续、设置重复/掉ACK、不接显示仍可鸣叫、BLE断连后采集继续、无profile前不触碰UART/USB冲突脚。Run `-Case brake_driver` Expected FAIL。

```cpp
DualBrakeProbe probe; BrakeHardwareConfig disabled{};
CHECK(!probe.begin(disabled)); CHECK(test_hw::uartBegins==0);
```

`test_hw::uartBegins`由此任务的`tests/wired_tire/stubs/BrakeHardwareStub.h`提供并由UART替身begin递增；只替换硬件API边界，执行真实新驱动方法。
- [ ] 双HardwareSerial(0/1)，CDC日志18/19，DE初始关闭；NVS适配版本双slot及读回/Unknown；SPI租约在DMA完成前保持；BLE名称/UUID固定，通知分步发送并恢复广播。保存时记录最大停顿，不假定Flash操作完全不阻塞。
- [ ] 默认offline false硬件、引脚-1，缺`ApprovedBrakeProfile.h`时hardware构建失败；保留启动/版本/脚位/电源/传感器/BLE/发送/报警/保存日志。WT_OFFLINE_LINK等效保留真实驱动路径。
- [ ] Run driver case及 `powershell -File scripts/compile_wired_tire.ps1 -Target brake`。Expected: PASS，ELF含UART半双工、NVS blob、SPI slave和BLE notify符号；编译不启用I/O。提交 `firmware: 添加六口底板双刹车控制固件`。物理GPIO/双UART/蜂鸣门槛保持待验证。

### Task 8: P4五从机总线与命令客户端

**Files:** Create display目录 `BoardSpiBus.h/.cpp`、`BrakeLinkMaster.h/.cpp`；Modify `SnapshotMaster.h/.cpp`、`DisplayConfig.h`；Create `test_board_spi.cpp`、`test_brake_client.cpp`及runner支持。
**Interfaces:** `BoardPeer{TireFL,TireFR,TireRL,TireRR,Brake}`；`BoardSpiBus::begin(const BoardBusConfig&)->bool`、`poll(uint64_t)`统一在途事务，`bool tryQueue(BoardPeer,const uint8_t* tx,uint8_t* rx,size_t len)`/`bool takeCompleted(BoardPeer,BusResult&)`；旧`SnapshotMaster::pollWheel`公开接口保留，内部使用同一总线。`BrakeLinkMaster(BoardSpiBus&)`、`bool poll(uint64_t,BrakeDelivery&)`、`SubmitResult submit(const ControlIntent&)`（Accepted/Busy/NotReady/Invalid）、`BrakeAck pendingResult() const`。客户端填入会话/命令ID/目标boot/实际配置版本，UI不自行伪造这些字段。
- [ ] 先测试四种1728与一种256混合轮转、READY低/错误token/peer复位、DMA超时不重用缓冲、某peer未就绪其余继续；`ack_lost_reconnect_reads_actual_never_replays_mutation`。Run `-Case board_spi`、`-Case brake_client` Expected FAIL。

```cpp
BoardSpiBus bus; BrakeLinkMaster client(bus);
ControlIntent intent{CommandOp::ApplySave,BrakeConfig{650,true}};
CHECK(client.submit(intent)==SubmitResult::NotReady);
```
- [ ] 非阻塞1MHz mode0，独立CS/READY；单请求窗口100ms、正常轮询250ms/重试500ms基线。超期在途内存仍等真实驱动完成，不制造取消成功；共享物理总线故障不能伪报其余正常。
- [ ] 命令只允许一个在途，同ID语义重试、不改操作/候选值；超过1000ms仍未确认则OutcomeUnknown，停止自动变更重发、先Read实际状态。发现任一端boot/配置版本变化清理旧写意图；读取可继续后台轮询。
- [ ] 重跑两caseExpected PASS并重跑原snapshot/view/driver回归；提交 `firmware: 接入五从机调度与刹车控制回执`。

### Task 9: 三页面纯UI、触摸动作与全局报警

**Files:** Create核心 `TelemetryUi.h/.cpp`、`RasterPrimitives.h/.cpp`、`test_telemetry_ui.cpp`、`test_telemetry_touch.cpp`；Modify `ThermalView.cpp`与display `HeatmapRenderer.h/.cpp`。
**Interfaces:** `enum class Page{Tire,Brake,Settings}`；`Page TelemetryUi::page() const`、`void ingest(const BrakeDelivery&,uint64_t)`、`UiAction touch(const TouchSample&,uint64_t)`、`void render(const std::array<wired_tire::SnapshotDelivery,4>&,uint64_t,const wired_tire::ColorScale&,wired_tire::PixelSurface&)`；`TouchSample{uint16_t x,y;uint8_t count;bool pressed,valid;uint64_t sampledUs;}`。`UiAction{UiActionKind kind;ControlIntent intent;}`，kind=None/Navigate/DraftChanged/Apply/Mute，只为意图由任务8执行。`wired_tire::raster::fill(PixelSurface&,size_t x,size_t y,size_t w,size_t h,uint16_t)`与`text(PixelSurface&,size_t x,size_t y,const char*,unsigned scale,uint16_t)`是供两种UI复用的void绘制函数。
- [ ] 先写四热图全部768点/镜像/无效位在导航和报警条之外仍映射、三页面边界哨兵；默认胎温页；所有页面ALARM可见；估算值与实测值标签区分；控制器失联不显示OK。
- [ ] 先写 `editing_draft_does_not_change_effective_alarm`、`saved_only_after_matching_ack`、`unknown_result_readback`、`new_brake_boot_drops_draft_write`、`held_multitouch_stale_touch_cannot_repeat_save_or_mute`。Run `-Case telemetry_ui`及`-Case telemetry_touch` Expected FAIL。

```cpp
TelemetryUi ui; CHECK(ui.page()==Page::Tire);
TouchSample twoFingers{10,10,2,true,true,0};
CHECK(ui.touch(twoFingers,0).kind==UiActionKind::None && ui.page()==Page::Tire);
```
- [ ] 保留旧renderDashboard API作原离线兼容，抽出字体/栅格内部工具给新UI；新内容区域按可用空间缩放，确保四幅32×24完整，不能照搬原320×240占满后裁掉行。设置控件范围/步进与真实配置一致，未接收新鲜状态前禁写。
- [ ] 触摸先确认50ms稳定、释放后才再次触发；草稿±10可500ms后每200ms连调但不写控制器；Mute长按1秒仅一次。sampledUs使用P4本地采样时刻，超过200ms未有有效报告即取消按压；多点/越界/回退时刻不触发。Apply pending锁编辑提交，切页与底层任务解耦，连续按压可用性须实屏复测。
- [ ] 重跑两caseExpected PASS，导出标SIMULATED DATA的三页预览用于检查，不称为真屏测试；提交 `firmware: 添加胎温刹车触屏页面与设置状态机`。

### Task 10: 7B板级适配、完整P4入口与隔离构建

**Files:** Create display `Board7BProfile.h`、`Board7BPanel.h/.cpp`、`Board7BTouch.h/.cpp`，核心`Board7BRules.h/.cpp`、`Gt911TouchCodec.h/.cpp`；必要底层第三方文件与LICENSE置于`firmware/wired_tire_display/src/vendor/`，只随P4 sketch构建，不放入纯核心/C3库；Modify `WiredTireCore.h`、`PanelDriver`适配接口、display `.ino`、build/link脚本；Create `test_board_profile.cpp`（同时覆盖GT911解码）。
**Interfaces:** `bool Board7BPanel::begin(const Board7BProfile&)`、`uint16_t* drawBuffer()`、`bool present(const uint16_t*,size_t)`保持面板契约；`TouchSample Board7BTouch::poll(uint64_t)`。核心`Board7BPinPlan{array<int,3> bus;array<int,5> cs,ready;bool electricalApproved,strapApproved,vo4Approved;}`与`ProfileResult validateBoardPins(const Board7BPinPlan&)`；`bool decodeGt911Report(uint8_t status,const uint8_t* points,size_t length,TouchSample&)`。硬件批准与本地编译开关分开，不因编译成功设置approved。
- [ ] 先测试SPI含32/33/26默认值、重复脚、未核准34/36绑带或46–48域时在I/O前拒绝；GT911坐标0/边缘有效，超过1023/599、多点及损坏状态不触发操作。Run `-Case board_profile` Expected FAIL。

```cpp
// 仅为纯validator测试输入；这些批准标志不代表真实板已核准，也不导出为接线表。
Board7BPinPlan p{{2,3,4},{5,28,29,30,31},{49,50,51,52,47},true,true,true};
CHECK(validateBoardPins(p)==ProfileResult::Ok);
p.bus[1]=32; CHECK(validateBoardPins(p)==ProfileResult::Reserved);
```

ProfileResult定义Ok/Unapproved/Unassigned/Duplicate/Reserved/NeedsStrapReview/NeedsVo4Review；每个拒绝原因分别测试，不能让其他错误掩盖目标断言。
- [ ] 依据任务1的锁定底层驱动实现EK79007/GT911、32反相背光、33复位、I²C7/8；保留触摸RST/INT差异的profile核准，不随意驱动23。双帧缓冲刷新租约和首帧后背光启用保留。触摸I²C单次等待有界10ms，避免拖住SPI或UI。
- [ ] 完整P4 `.ino` 调度五peer、UI、触摸、命令回执和日志；不让页面函数承担后台采集/报警。默认仍disabled，硬件profile必须来自真实审查。
- [ ] `compile_wired_tire.ps1 -Target display -ChipVariant prev3`及`postv3`，FQBN显式PSRAM=enabled。Expected: 各独立build目录编译/ELF链接PASS；若3.3.7与锁定驱动确有不兼容，保存错误后在独立Arduino data目录安装3.3.11仅给P4，并更新版本清单与同样矩阵。不得直接用latest或更改旧全局core。
- [ ] 重跑board_profile及UI回归Expected PASS，提交 `firmware: 适配微雪7B显示触摸与六口主控入口`。真实芯片rev、VO4/绑带、触摸校准、启动/断电和30FPS保持未通过。

### Task 11: 硬件及手机关卡（无实物保持未完成）

**Files:** Modify `notes/validation_matrix.md`、`validation_spi.md`、`validation_ble.md`、`validation_display.md`、`validation_power.md`、`validation_vehicle.md`；Create `validation_brake.md`、`validation_settings.md`、`resource_approval.md`。
**Interfaces:** 消费固件版本、锁定资料与实际板号/接线，输出带日志/仪器/手机版本的证据；不是由合成用例推断PASS。
- [ ] 实际7B chip/rev/VO4、第五C3 GPIO/USB/DE启动、13脚无冲突、供电/接口防反灌均核准后才建立Approved profiles和台架接线；先限流供电，不拿车电直接试短路。
- [ ] 胎温原A–F和双刹车协议/负温/失败恢复、原地址或真实出厂地址、独立线路故障、模型和蜂鸣逐项复测；P4断开/重启/触摸失效时后台报警仍运行。
- [ ] 目标手机连接四胎温＋N BrakeTemp，至少60min，分别改变六测点、遮挡、断连/恢复，检查轮位/单位/无效值与五设备共存；记录实际RaceChrono计算表达式。至少10min六探头/五BLE/触屏同时运行，测实际绘制/提交间隔、最大卡顿与热源延迟。
- [ ] 枚举设置写前/中/后断电、丢/重复ACK、双MCU重启、存储故障；验证完整旧/新配置和未知提示、ALM只复位一次、静音不延长。测NVS最大停顿；至少30次点火循环并核对启动压降、完整线束及机械安装，不以开关试验代替汽车浪涌验证。
- [ ] Expected: 每项有可追溯实测记录才勾选；缺实物仅补实验步骤，保持NOT RUN。只有证据满足后才冻结六口原理图/PCB/制造物料；按完成范围提交记录，不生成假BOM/Gerber。

### Task 12: CI、完整交付、文档和最终审查

**Files:** Modify `.github/workflows/project-checks.yml`、compile/link/native/package脚本、`tests/wired_tire/test_package.py`、README、CODEX、docs04/08/11/12/13、三个新方向sketch README及notes；Create `notes/validation_six_port_offline.md`。
**Interfaces:** 包含完整 channel/brake/display sketches、共享库ZIP、固定依赖/构建说明、接口/轮位表、资源证据及验证范围；MANIFEST逐文件哈希，硬件未通过则hardware_validated=false。
- [ ] 先写包测试：缺刹车入口/任一驱动失败、三个完整目录及vendor许可入包、Approved profiles/缓存不导出、模型/线协议声明齐全、旧四轮ZIP不覆写。Run Python `test_package.py` Expected 新要求FAIL。

```python
self.assertTrue((package / 'firmware/wired_brake_channel/wired_brake_channel.ino').is_file())
self.assertFalse(manifest['hardware_validated'])
```
- [ ] 更新打包/安装（含新刹车只读抓包脚本）、三个目标编译矩阵（display含prev3/postv3），C3 brake显式CDC；旧固件列举任务保留，原有59项命名用例和10项工具用例作为回归基线而非新通过数。文档保护断言分别覆盖Rejsa与DIY CAN，不只写“通道不变”。
- [ ] Run native `-Case all`、`python -m unittest discover -s tests/wired_tire -p 'test_*.py'`、三目标编译/符号检查、`python scripts/check_docs_consistency.py`、`git diff --check`。Expected 全PASS并记录实际数量/版本；失效项先定位，不能删检查求绿。
- [ ] 最终源码提交后生成新版本离线ZIP，另行解压到新目录校验全部SHA256并运行原生/Python及三目标编译，确认不靠原目录隐藏文件。实际帧率、射频、持久化掉电和汽车保护仍按任务11报告，不替换成离线PASS。
- [ ] 按原生执行技能要求做一次最强可用模型的全分支只读新上下文审查，提供本计划Review Focus、规格和Ruling；重要问题一次复现测试RED→GREEN修复并全回归，保留需硬件验证的判断。提交 `docs: 完成六接口中控离线交付与验证说明`；更新分支并给完整文件路径。

## 自审与交接

覆盖：规格1–3→任务1/2/7/10；4→任务3/6/7；5→任务4/9；6→任务5/8；7→任务6/12；8→任务11/12。Review Focus各项已在所属任务加入对应输入，不把硬件未知当作测试可证明的事实。接口统一为`wired_brake`与`BrakeDelivery`，胎温仍为原`wired_tire::SnapshotDelivery`；不构造WheelId=5。新模块/板间/界面存在顺序依赖，作为一份增量集成计划执行，不并行冻结其共享接口。

本计划已获用户确认；原生执行选择保留，已进入executing-plans，逐任务在同一隔离工作区执行；无实物时完成离线部分并留存对应硬件关卡。

依赖顺序：任务1先完成资料/版本的离线部分，任务2–10按接口先后实现，任务11是硬件放行条件而非离线打包前提，任务12可交付明确标为未实物验证的软件包。每个带实物步骤的任务只记录已完成部分；不能因准备了说明就把任务11勾成通过。
