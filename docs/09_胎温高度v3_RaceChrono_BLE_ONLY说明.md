# 09 胎温高度 v3 RaceChrono BLE ONLY 固件说明

本文件记录当前用户最满意的一版胎温高度节点 v3 固件方向：`tire_temp_height_node_racechrono_fast_clean_v3.ino`。

## 1. 当前结论

胎温高度节点 v3 现在不再向中控发送胎温数据。

当前定位是：

```text
ESP32-C3 + MLX90640 + VL53L1X
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

不再走：

```text
ESP-NOW -> 中控
WiFi -> 中控
UDP/TCP -> RaceChrono
Central display link
```

固件启动日志中也应明确：

```text
BLE only. No WiFi, no ESP-NOW, no central display link.
```

---

## 2. 最高优先级

胎温高度 v3 的最高优先级是 RaceChrono 数据质量：

1. RaceChrono 能稳定搜索、连接、记录。
2. 胎温热力分布要跟手，热源移动后热图变化不能明显拖延。
3. 温度不能有很夸张的抖动。
4. 不能为了追求极限刷新率导致 MLX90640 频繁 I2C 错误。
5. 不能为了过度平滑导致热力图严重滞后。
6. 不能因为中控显示或 ESP-NOW 逻辑影响 BLE 发送节奏。

如果功能扩展和 RaceChrono 数据质量冲突，优先 RaceChrono。

---

## 3. 当前满意固件的关键特征

### 3.1 BLE only

当前固件只保留 RaceChrono BLE 输出：

- 不使用 WiFi
- 不使用 ESP-NOW
- 不向中控发送胎温数据
- 不依赖 central display

### 3.2 RejsaRubberTrac 风格 BLE

当前固件使用 RejsaRubberTrac 风格 BLE：

```cpp
const char* DEVICE_NAME = "RejsaRubberRL123456";
```

轮位 ID：

```cpp
// 1=FL, 2=FR, 3=RL, 4=RR
const uint8_t TIRE_ID = 3;
```

BLE Service / Characteristic：

```text
Service UUID: 0x1FF7
Characteristic 1: 0x0001
Characteristic 2: 0x0002
Characteristic 3: 0x0003
```

不要随意改这些内容，否则 RaceChrono 可能搜索不到或数据解析异常。

### 3.3 MLX90640 快速但带保护

当前固件优先启动 FAST profile：

```text
I2C_FAST_CLOCK = 1MHz
MLX refresh = 32Hz
ADC = 17bit
FRAME_FAST_INTERVAL_MS = 34ms
```

如果 I2C 总线不稳定，会自动回退：

```text
I2C_STABLE_CLOCK = 400kHz
MLX refresh = 16Hz
ADC = 18bit
FRAME_STABLE_INTERVAL_MS = 70ms
```

不要删除自动 recover / fallback 逻辑。

### 3.4 BLE 发送节奏

当前固件 BLE 通知节奏：

```text
SEND_INTERVAL_MS = 30ms
约 33Hz BLE notifications
```

发送前会判断：

- BLE 是否连接
- 是否已有有效温度
- 温度是否超时 stale
- 连接后是否已等待约 800ms

这些保护不要删除。

### 3.5 胎温清洗和滤波

当前固件对 MLX90640 温度处理顺序：

```text
有效范围剔除
trimmed mean 分区均值
rate limit 限制变化速度
adaptive low-pass 自适应低通
deadband 小变化死区
spike gate 抑制离谱跳变
```

关键参数：

```text
TEMP_MIN_C = -20°C
TEMP_MAX_C = 200°C
TEMP_DEADBAND_C = 0.06°C
TEMP_MAX_RATE_C_PER_S = 160°C/s
RAW_SPIKE_GATE_C = 12°C
TEMP_STALE_MS = 1500ms
```

这些参数关系到“跟手”和“不夸张抖动”的平衡，不能随意改。

### 3.6 ROI 自动跟踪

当前固件使用 32×24 MLX 全帧中自动寻找 ROI：

```text
FRAME_ROWS = 24
FRAME_COLS = 32
ROI_HEIGHT = 8
ROI_SEARCH_MIN_ROW = 3
ROI_SEARCH_MAX_ROW = 17
ZONE_COUNT = 8
```

ROI 跟踪参数：

```text
ROI_TRACK_ALPHA = 0.35
MAX_ROI_STEP_PER_FRAME = 1
```

ROI 变化有限速，避免热区突然跳到别的行导致热图乱跳。

### 3.7 高度传感器仍保留

虽然不再向中控发送胎温，高度数据仍在 BLE 包里保留：

- VL53L1X
- Short mode
- 20Hz 高度读取
- 启动后自动 baseline
- 单击按键可重新校准高度零点

---

## 4. 当前 v3 固件典型引脚

```cpp
const int SDA_PIN          = 8;
const int SCL_PIN          = 10;
const int VBAT_ADC_PIN     = 1;
const int LED_STAT_PIN     = 2;  // Active low
const int BTN_PIN          = 3;  // Active low
const int PWR_KILL_PIN     = 4;  // HIGH keeps power on
const int PWR_INT_PIN      = 5;
const int VL53_XSHUT_PIN   = 6;
const int VL53_INT_PIN     = 7;
```

不要随意修改这些引脚。

---

## 5. 按键行为

当前固件按键行为：

```text
单击：高度归零 / 校准 travel baseline
双击：切换胎温区域镜像 mirror zones
长按：关机
```

相关参数：

```text
BTN_DEBOUNCE_MS = 8
BTN_DOUBLE_CLICK_MS = 650
BTN_LONG_PRESS_SHUTDOWN_MS = 1800
STARTUP_IGNORE_BUTTON_MS = 1200
```

不要让按键扫描阻塞 BLE 或 MLX 更新。

---

## 6. Codex 修改 v3 固件时必须遵守

1. 不要重新加入 ESP-NOW 胎温发送。
2. 不要重新加入中控胎温发送链路。
3. 不要加入 WiFi / UDP / TCP 外部遥测替代当前 BLE。
4. 不要破坏 RejsaRubberTrac 风格 BLE 设备名、Service、Characteristic 和数据包布局。
5. 不要牺牲 RaceChrono 热力图跟手性。
6. 不要为了平滑温度而造成热图明显延迟。
7. 不要为了刷新率而删除 MLX90640 recover / fallback 保护。
8. 不要删除温度 valid-range、trimmed mean、rate limit、adaptive low-pass、spike gate。
9. 不要把故障帧、stale 温度或无效温度继续发送成正常数据。
10. 不要修改 v3 电源控制相关引脚。
11. 修改后必须说明是否影响 RaceChrono 搜索、连接、热力图显示和记录。
12. 修改后必须说明是否影响热力分布跟手性和温度抖动。

---

## 7. 推荐测试流程

### 7.1 台架测试

1. 烧录固件。
2. 串口确认启动日志出现：

```text
Boot RaceChrono fast clean V3 firmware
BLE only. No WiFi, no ESP-NOW, no central display link.
```

3. 确认 MLX profile 启动为 FAST_CLEAN 或稳定回退到 ROCK_STABLE。
4. 确认 VL53L1X 初始化成功。
5. RaceChrono 搜索到 RejsaRubber 设备。
6. RaceChrono 连接后有胎温数据。
7. 热源横向移动时，热力分布应跟手移动。
8. 热源固定时，温度不应夸张抖动。
9. 断开 BLE 后应重新 advertising。

### 7.2 车上测试

1. 实车安装后，RaceChrono 能稳定连接。
2. 轮胎经过热区变化时，热图变化不能明显滞后。
3. 连续运行 20~30 分钟不掉线。
4. 震动环境下 MLX 不应频繁 recover。
5. 如果回退到 ROCK_STABLE，数据仍应稳定可用。

---

## 8. 当前参考固件文件

用户当前最满意的一版固件文件名：

```text
tire_temp_height_node_racechrono_fast_clean_v3.ino
```

该文件应作为胎温高度 v3 RaceChrono BLE only 的重要参考版本。

后续如果上传到仓库，建议放到：

```text
firmware/tire_temp_height_node_v3/tire_temp_height_node_racechrono_fast_clean_v3.ino
```
