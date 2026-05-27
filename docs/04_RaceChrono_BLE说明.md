# 04 RaceChrono BLE 说明

## 1. RaceChrono 在本项目中的作用

RaceChrono Pro 是本项目的重要数据记录平台。

本项目希望通过 RaceChrono 实现：

- 轮胎温度记录
- 胎温热力图显示
- 刹车温度记录
- 悬挂高度 / 行程数据记录
- 圈速数据与温度数据同步
- 后期视频叠加和分析

## 2. iOS RaceChrono 的重要限制

本项目主要面向 iOS RaceChrono Pro。

iOS 版本相对 Android 在自定义外部遥测方面存在更多限制。

已知经验：

- 原生胎温热图更适合走 RejsaRubberTrac 风格 BLE 设备。
- Wi-Fi UDP / TCP 外部遥测在 iOS 上配置和通道显示受限。
- DIY CAN BLE 可以用于某些自定义通道，但名称和显示方式受限制。

因此，不要轻易把已经可用的 RejsaRubberTrac BLE 方案换成 UDP/TCP。

## 3. BLE 设备命名

胎温节点常见命名示例：

```cpp
const char* DEVICE_NAME = "RejsaRubberFL123456";
const char* DEVICE_NAME = "RejsaRubberFR123456";
const char* DEVICE_NAME = "RejsaRubberRL123456";
const char* DEVICE_NAME = "RejsaRubberRR123456";
```

轮位建议：

```cpp
// 1 = FL
// 2 = FR
// 3 = RL
// 4 = RR
const uint8_t TIRE_ID = 3;
```

不要随意修改设备名格式，否则 RaceChrono 可能搜索不到或无法按预期识别。

## 4. RaceChrono 修改风险

修改以下内容可能导致 RaceChrono 无法识别：

- BLE 设备名
- Service UUID
- Characteristic UUID
- 广播参数
- 数据包长度
- 数据顺序
- 轮位 ID
- 发送频率

修改前必须明确说明风险。

## 5. 数据刷新率原则

胎温和高度数据希望尽可能低延迟，但不能牺牲稳定性。

建议：

- MLX90640 读取频率根据稳定性选择 4Hz / 8Hz
- BLE 发送不要过于频繁导致掉线
- 传感器读取失败时不要发送误导数据
- 显示刷新和 BLE 发送应分开节奏

## 6. BLE 优先级

RaceChrono BLE 连接是关键功能。

代码中应避免：

- 大量阻塞 `delay()`
- 在 loop 中长时间卡住
- 传感器失败后死循环
- 显示刷新阻塞通信
- 按键逻辑阻塞 BLE

## 7. 推荐测试流程

每次修改 BLE 相关代码后，应测试：

1. 烧录后串口确认 BLE advertising started。
2. iPhone 打开 RaceChrono Pro。
3. 搜索外部设备。
4. 确认设备名出现。
5. 连接设备。
6. 观察是否有温度数据。
7. 轻微移动热源，确认热图 / 数据有响应。
8. 保持连接 10 分钟以上，观察是否掉线。
9. 断开重连，确认可恢复。

## 8. 常见问题

### 8.1 RaceChrono 搜索不到设备

可能原因：

- BLE 没有启动
- 设备名不符合预期
- 手机蓝牙缓存问题
- 固件卡在传感器初始化
- ESP32 供电不稳
- 代码中阻塞过久

### 8.2 RaceChrono 连接后无数据

可能原因：

- Characteristic 没有 notify
- 数据包格式错误
- 传感器读取失败
- 发送频率异常
- RaceChrono 通道配置不正确

### 8.3 之前能连，后来不能连

可能原因：

- RaceChrono App 数据过多或状态异常
- 手机蓝牙缓存
- 固件名称重复
- 设备没有真正重启
- 电源电压不稳

历史经验中，RaceChrono App 端状态异常也可能导致连接问题，不能只怀疑硬件。
