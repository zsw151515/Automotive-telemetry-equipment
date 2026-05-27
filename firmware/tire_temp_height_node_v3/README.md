# 胎温高度节点 v3 固件（RaceChrono BLE only 当前主线）

## 用途

用于胎温高度节点 v3 量产候选硬件：

- ESP32-C3
- MLX90640
- VL53L1X
- LTC2954-2 单按键电源控制
- RaceChrono / RejsaRubberTrac compatible BLE only

## 当前状态

```text
当前状态：当前主线 / 用户最满意版本方向
参考固件：nice_racechrono_fast_clean_v3.ino.ino
```

## 当前软件定位

胎温高度节点 v3 当前不再向中控发送胎温数据。

当前链路：

```text
MLX90640 胎温 + VL53L1X 高度
        ↓
本地清洗 / 滤波 / ROI / 分区
        ↓
BLE RejsaRubberTrac 风格数据包
        ↓
RaceChrono Pro
```

不再使用：

```text
ESP-NOW -> 中控胎温
WiFi -> 中控
UDP/TCP -> RaceChrono
Central display link
```

## 最高优先级

- RaceChrono 数据准确
- 热力分布跟手
- 温度不过度抖动
- BLE 连接稳定
- MLX90640 出错后可恢复
- 不因额外功能影响 BLE 发送节奏

## 当前满意固件关键点

- `DEVICE_NAME = "RejsaRubberRL123456"`
- Service UUID：`0x1FF7`
- Characteristic：`0x0001`、`0x0002`、`0x0003`
- FAST profile：1MHz I2C、MLX90640 32Hz、17bit ADC
- STABLE fallback：400kHz I2C、MLX90640 16Hz、18bit ADC
- BLE 通知约 30ms 一次
- 温度处理包含 valid-range、trimmed mean、ROI tracking、rate limit、adaptive low-pass、spike gate
- 高度数据仍保留在 BLE 包内

## 关键硬件结论

- U_PWR 应使用 `LTC2954CTS8-2#TRPBF`
- `R_BTN_PULL` 应 DNP / 删除
- `PWR_KILL -> SYS_3V3` 需要 47kΩ 上拉
- 不要随意修改 `PWR_KILL`、`PWR_INT`、按键和电源保持逻辑

## 典型引脚

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

## Codex 修改规则

- 不要重新加入 ESP-NOW 胎温发送。
- 不要重新加入中控胎温发送链路。
- 不要加入 WiFi / UDP / TCP 外部遥测替代当前 BLE。
- 不要破坏 RejsaRubberTrac 风格 BLE 设备名、Service、Characteristic 和数据包布局。
- 不要牺牲 RaceChrono 热力图跟手性。
- 不要为了平滑温度而造成热图明显延迟。
- 不要为了刷新率而删除 MLX90640 recover / fallback 保护。
- 不要删除温度 valid-range、trimmed mean、rate limit、adaptive low-pass、spike gate。
- 不要把故障帧、stale 温度或无效温度继续发送成正常数据。
- 修改后必须说明是否影响 RaceChrono 搜索、连接、热力图显示和记录。
- 修改后必须说明是否影响热力分布跟手性和温度抖动。

## 上传固件后需要补充

- Arduino IDE / ESP32 core 版本
- 依赖库版本
- RaceChrono 设备名
- 轮位 ID
- 实测连接稳定性
- 实测热图跟手性
- 实测温度抖动情况
