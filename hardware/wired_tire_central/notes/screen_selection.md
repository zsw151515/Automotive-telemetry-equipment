# 7 英寸屏幕选型状态

2026-10-24：用户选择 **微雪 ESP32-P4-WIFI6-Touch-LCD-7B 完整显示套件＋六接口底板，同一外壳**，四胎温＋两前刹车。触屏替代原刹车小屏/设置键，切页仍保持后台采集、报警及五 BLE。以 `docs/superpowers/specs/2026-10-24-six-port-tire-brake-7b-design.md` 为最新设计说明；此前四口、自行设计 P4 同板和 5 英寸方案为历史。

[官方文档](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B)列出 7 英寸横屏 1024×600、MIPI-DSI、P4/C6、32 MB PSRAM，以及 PH2.0 两组 12 针扩展接口。不能使用 5 英寸板的 40 针定义。

状态：型号和装配形态已选定，尚无真实 7B 验证，不存在核准接线、面板 profile 或已测 30 FPS。后续依据[官方资源](https://docs.waveshare.com/ESP32-P4-WIFI6-Touch-LCD-7B/Resources-And-Documents)记录实际板/芯片修订、BSP、供电、背光/复位及机械固定。现有代码仍为四轮 11 GPIO，六口工程基线需 13 GPIO，需核查实际占用、启动约束和板间时序；触屏导航、设置和报警状态还需适配。

离线 PanelConfig 仍未核准，hardwareEnabled=false；选定套件不等于驱动已实现。先确认官方显示原型和 1024×600 双缓冲提交，再迁移应用。日光可读性、温升、屏幕背板对四个 C3 天线的影响、点火自动开机/断电及快速重启均须实测。

硬件验证关卡未通过前，不因模拟界面看起来正常而生成投板用屏幕接口或承诺 30 FPS。
