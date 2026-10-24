# 7 英寸屏幕选型状态

2026-10-24：用户明确选择 **微雪 ESP32-P4-WIFI6-Touch-LCD-7B 完整显示套件＋四接口采集底板，同一外壳**。以 `docs/superpowers/specs/2026-10-24-wired-tire-7b-kit-design.md` 为本次结构变更说明；此前自行设计 P4/屏幕同板和讨论中的 5 英寸方案不作为后续方向。

[官方文档](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B)列出 7 英寸横屏 1024×600、MIPI-DSI、P4/C6、32 MB PSRAM，以及 PH2.0 两组 12 针扩展接口。不能使用 5 英寸板的 40 针定义。

状态：型号和装配形态已选定，尚无真实 7B 板级验证，不存在已核准的接线、面板 profile 或已测 30 FPS。后续记录实际板/芯片修订、原配面板与 BSP 版本，依据[官方资源](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B/Resources-And-Documents)核对供电入口、背光/复位、GPIO 和机械固定。现有 SPI 主机需 11 GPIO，需检查实际占用、启动约束和板间电气时序。

离线 PanelConfig 仍未核准，hardwareEnabled=false；选定套件不等于驱动已实现。先确认官方显示原型和 1024×600 双缓冲提交，再迁移应用。日光可读性、温升、屏幕背板对四个 C3 天线的影响、点火自动开机/断电及快速重启均须实测。

硬件验证关卡未通过前，不因模拟界面看起来正常而生成投板用屏幕接口或承诺 30 FPS。
