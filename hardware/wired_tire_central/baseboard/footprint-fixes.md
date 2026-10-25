# 本工程封装修正与使用边界

以下修改仅作用于六接口底板工程本地库，不修改共享库或EDA安装目录。已导出原生工程并重新打开PCB复核；仍须实物装配和制造工艺确认。

## 六个探头连接器

J5～J10采用Molex 430450427的原厂尺寸图核对。四个接触孔及两个定位孔均为1.02±0.05mm；原库的1.20mm孔已修正为1.02mm。保持接触焊盘铜环、编号、位置及两个定位孔，未因个别批次只有一个定位柱而删孔。

原厂图中的本体约9.65×8.77mm，高度约9.91mm；与线端配合后约17.64mm。安装在底面时，从PCB顶面看焊盘位置会镜像，必须按配合面视图核对压接次序，不能根据屏幕截图猜线色。

参考：[Molex 430450427尺寸图](https://www.molex.com/content/dam/molex/molex-dot-com/products/automated/en-us/salesdrawingpdf/430/43045/430450427_sd.pdf)。

## S3散热焊盘

U5的41号地焊盘保留九块分割铜及原有锡膏形式，12个网格间地过孔保持原孔位，修正为GND、0.30mm孔径、0.50mm外径。原库为0.25/0.40mm且无网名；当前设计规则要求至少0.30mm孔、0.50mm外径，因此修正封装，未降低检查标准。

PCB实例的过孔孔径读回在本客户端会取整到mil，不能据此反向修改封装。实际尺寸以原生源码、制造输出和DRC为准。焊盘内及近焊盘过孔须结合填孔和锡膏工艺评审；盖油与填孔是不同工艺。

参考：[乐鑫S3布局指南](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/pcb-layout-design.html)、[嘉立创过孔覆盖说明](https://jlcpcb.com/help/article/pcb-via-covering)。

## 7B及维护排针

7B的P1与P3均为12位PH2.0接口。底板J3/J4相应为12位线束接口；J11～J15的六位排针仅用于原生USB维护，不连接7B的P3。

当前选定库料为PH系列兼容型号，不能因2mm节距就声称其全部尺寸与JST原厂型号一致。若采购改用JST，必须对照B12B-PH-K-S及B6B-PH-K-S的孔径、定位和配合高度重新核对；当前六位维护排针库孔径约0.914mm，不能当作已符合JST推荐孔径。维护排针通常不装配线束，维修时打开后盖使用。

屏幕电源J2为独立XH三位接口，与PH信号/维护接口分开。

参考：[微雪7B说明](https://docs.waveshare.net/ESP32-P4-WIFI6-Touch-LCD-7B/)、[JST PH尺寸资料](https://www.jst-mfg.com/product/pdf/eng/ePH.pdf)。
