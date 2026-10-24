# 7B 官方资料核对（六接口计划输入）

日期：2026-10-24。范围：公开资料及本机 SDK 的只读核对；没有实物测量，本文不是接线核准或制造文件。

## 来源与固定版本

- [用户提供的中文说明书](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-7B/)。
- [官方主板 PDF](https://www.waveshare.net/w/upload/e/e3/ESP32-P4-WIFI6-Touch-LCD-7B.pdf)：第 1 页原理图，第 2 页丝印。
- [官方 BSP 引脚](https://github.com/waveshareteam/Waveshare-ESP32-components/blob/d7567970be6d6a94077727eb69d580d63a898eab/bsp/esp32_p4_wifi6_touch_lcd_7b/include/bsp/esp32_p4_wifi6_touch_lcd_7b.h)、[BSP 依赖](https://github.com/waveshareteam/Waveshare-ESP32-components/blob/d7567970be6d6a94077727eb69d580d63a898eab/bsp/esp32_p4_wifi6_touch_lcd_7b/idf_component.yml)，提交 `d7567970be6d6a94077727eb69d580d63a898eab`。
- [官方 Arduino 示例](https://github.com/waveshareteam/ESP32-P4-WIFI6-Touch-LCD-7B/blob/9fcd49a8c927a00f5583c22d568b3448addbe51c/examples/arduino/README_ZH.md)，提交 `9fcd49a8c927a00f5583c22d568b3448addbe51c`。

## 扩展接口与 GPIO 限制

按官方 PDF 的连接器顺序摘记，必须再与实际板版本/连接器方向核对：

| 排针 | pin1 → pin12 |
| --- | --- |
| P3 | GPIO36、34、31、30、29、28、5、4、3、2、GND、3V3 |
| P1 | GPIO52、51、50、49、48、47、46、GND、VO4、3V3、GND、BAT |

17 个引出的可编程 GPIO 不是 17 个可以无条件接 3.3V SPI 的脚。34/36 涉及启动绑带，46–48 涉及 VO4 电压域及 SD 相关配置；排除这些脚后只有 12 个，六口设计的 13 信号方案须核准至少一个相关引脚，或重新审阅接口设计。

在未确认实际 VO4 电压/初始化及绑带条件前，不给出可施工的 13 脚分配。SPI 必须显式指定所有信号，不能使用本机通用 P4 variant 的默认 MOSI32/MISO33/SS26。

## 显示、触摸及供电

官方 BSP 使用 EK79007 双 lane MIPI-DSI；GPIO32 是反相背光 PWM（5 kHz、10 bit），GPIO33 是屏幕复位。它们与上述默认 SPI 脚冲突。

GT911 使用 I²C GPIO7/8。原理图触摸复位连接 GPIO23、INT 到测试点；BSP 的触摸 RST/INT 却均设 NC，并轮询 0x5D/0x14。此差异保留到实际版本核对，不照注释或其他套件猜测复位方式。

板图为 5V 输入、机械电源开关，含板上稳压/充电/电池升压及屏专用电源。外接车辆 12V 必须先经匹配保护和稳压；不套用其他板的 PMIC/PWR_KILL 接法。USB 和电池旁路、点火重启、峰值电流及热耗散仍未测量。

## 构建边界

当前官方 Arduino 示例使用 core 3.3.11；BSP 3.0.1 声明 IDF≥5.5，并带 LVGL 9.5 等依赖。项目本机 core 3.3.7 内置 IDF5.5.2，不能由版本数字就推断整个 BSP 可直接使用。新程序优先复用现有纯绘制代码及经锁定的底层驱动，不引入未使用的音频/网络/整套 UI 依赖。

本机 boards.txt 提供 `ChipVariant=prev3/postv3` 和 `PSRAM=enabled`，其中 postv3 SDK 最低修订为 3.01；实际芯片必须先识别再选择构建。现有通用 offline P4 默认构建没有打开 PSRAM，不作为 7B 帧缓冲配置的依据。

实施先在独立构建目录验证现有 core 3.3.7 的双变体；若确有驱动 API/芯片修订不兼容的错误证据，再把显示侧隔离到固定 3.3.11，保留 C3 与旧固件环境。实际接线、面板启动、触摸坐标和 30 FPS 均待实物验证。
