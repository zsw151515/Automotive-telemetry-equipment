# 有线胎温中控显示主控

新定制中控 PCB 的 ESP32-P4 显示固件，与四个 C3 采集/BLE 单元通信。当前默认离线构建，SPI 与屏幕硬件均禁用。

编译：powershell -File scripts/compile_wired_tire.ps1 -Target display。没有核准接线时不能用本固件做实物连接。

实际 hardware profile 需要 ApprovedDisplayProfile.h，定义 display_config 命名空间中的 hardwareEnabled、sck、mosi、miso、selects（四个独立片选）、ready（四个就绪信号）。这些字段不能凭空填写，必须来自正式 pinmap 审查。

SPI 为 1 MHz mode 0、固定 1728 字节的全双工事务。每次主机发送相同长度的读取请求，并接收从机此前准备的响应；同一个 token 可在 100 ms 窗口内重试，直到收到匹配快照。P4 每次启动以随机种子开始 token，避免固定从 1 开始读取重启前的排队旧响应。通信由异步 DMA 推进，等待就绪不阻塞其他轮。

响应内的数据年龄加上主机请求耗时和接收后的本地经过时间，避免 SPI 重读续命。数据状态与 BLE 状态分别显示；物理 SPI、面板帧率和 RaceChrono 共存尚未验证。
