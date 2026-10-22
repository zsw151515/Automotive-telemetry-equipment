# 有线胎温单轮采集单元

目标为新中控 PCB 内的 ESP32-C3 单轮单元，读取成品 RS485 MLX90640；不是旧无线 v3 替换固件。

当前为离线研发，未连接探头。默认构建 WIRED_TIRE_OFFLINE_BUILD=1，只输出日志，传感器 I/O 和 BLE 均不启用。没有核准接线时不得把默认构建标成实物可用版本。

完整 sketch 包含 .ino、同目录头文件/实现和共享 WiredTireCore 库。编译：powershell -File scripts/compile_wired_tire.ps1 -Target channel。

真实接线必须创建经过审查的 ApprovedChannelProfile.h，并通过 -Profile hardware 显式选择；该文件定义 channel_config 命名空间中的 hardwareEnabled、wheel、rx、tx、direction。引脚应来自 notes/pinmap.md 的核准版本，不能借用旧 v3。未提供时硬件构建失败，不猜测引脚。

该 profile 还需定义 bleAuxVerified 和 wired_tire::LegacyAuxFields 类型的 auxiliaryFields。未经手机实测核准时 bleAuxVerified 必须为 false；BLE 不启动，不把全零辅助字段当作真实高度或电池。单轮/四轮实测见 notes/validation_ble.md。

SPI profile 还需定义 sck、mosi、miso、select、ready；初始化前检查与 UART 的引脚重复。使用默认 UART0 日志时保留 GPIO20/21，使用 USB 日志时也必须核对 USB 引脚占用。SPI 异步事务在完成前保持发送缓冲不可变，不能让传感器更新覆盖在途图像。

串口基线 115200 8N2，使用 UART1 的硬件半双工方向控制；只发送探头 01 的 0x65 读取请求。请求限时 400 ms，失败后退避 500 ms；正常请求周期 250 ms。真实探头参数、串口周转和方向控制必须实测。

保留启动、版本、轮位、引脚、供电来源、传感器初始化、响应与错误计数日志。不测量电源电压时明确记录未测量。初版仅使用编译期配置，不提供运行期配置写入或自动探头设置写入；持久化设置及掉电试验留在硬件关卡中。
