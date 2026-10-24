# 7B 六口资源与工具链审查

2026-10-24：仅公开资料/本地工具链核对，electricalApproved、strapApproved、vo4Approved 均为 false。不能据此接线。

固定资料及图纸证据见 [原始资料审查](7b_source_audit.md)。P3 为 GPIO36/34/31/30/29/28/5/4/3/2/GND/3V3；P1 为 GPIO52/51/50/49/48/47/46/GND/VO4/3V3/GND/BAT。34/36 属绑带，46–48 的 VO4 电平及 SD 复用需核实，排除这五脚只剩12脚。五从机需要 SCK/MOSI/MISO＋5CS＋5READY＝13脚，因此资源没有无条件闭合；不得把测试中的合成 profile 当接线表。

固定外设：背光32反相、5kHz/10bit；屏复位33；GT911 I²C SDA7/SCL8。通用 P4 MOSI32/MISO33/SS26 不适用。触摸原理图标 RST23、INT测试点，而固定 BSP 使用无复位/中断轮询，保留差异，未核准前不驱动23。无通用 PWR_KILL/PMIC 关机方案。

第五 C3 预算：双UART RX/TX/DE共6脚，SPI SCK/MOSI/MISO/CS/READY共5脚，蜂鸣1脚，共12脚；USB CDC独占18/19。UART0上电日志可能输出，必须用外部下拉/收发器使能门控让两路DE上电关闭，不能靠setup补救。Flash、绑带、实际模块可引脚均待审查；软件默认所有脚-1、I/O禁用。

本机 Arduino CLI 1.5.0，ESP32 core 3.3.7 / IDF5.5.2。DisplayCoreVersion=3.3.7（首选，不升级全局环境）。C3新刹车目标显式 `esp32:esp32:esp32c3:CDCOnBoot=cdc`；P4分别构建 `esp32:esp32:esp32p4:ChipVariant=prev3,PSRAM=enabled` 与 `ChipVariant=postv3,PSRAM=enabled`。postv3 SDK最低silicon revision为3.01；两种编译均不能识别未连接的实际芯片。只有真实驱动/实际芯片不兼容证据成立才隔离固定3.3.11显示环境。

放行需要：实际板号/芯片修订、13脚资源、电平/VO4、绑带启动、触摸复位差异、第五C3双UART与USB、DE上电、5V供电及反灌逐项实测。缺任何项时保持 profiles 禁用。
