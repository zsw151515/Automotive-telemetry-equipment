# 六接口底板 V0.1 工程样机

本目录对应嘉立创EDA专业版独立工程“六接口中控底板 V0.1 工程样机”。目标是同壳安装7B套件、四路胎温RS485、两路前刹车RS485及蜂鸣报警。图纸仍处工程设计阶段；是否完成网表、布局和审查，以 `review.md` 和实际导出文件为准，不能直接用于下单。

2026-10-25 阶段快照：已建立12页原理图、282个器件、150×75mm四层PCB布局及首批降压回路走线。原理图1036个已定义引脚网络、PCB1170个实体焊盘网络及器件位置已分别核对。当前PCB仍有990项未连接提示；主供电分配、信号布线和制造审查继续进行。

## 工程资料

- `parts.json`：精确器件型号、厂家、立创编号、官方库UUID、符号与封装引用；不表示实时库存。
- `schematic-model.json`：各器件位号、图页、焊盘到网络映射及DNP配置。原理图坐标单位为10mil，PCB坐标单位为mil。
- `pinmap.csv`：本次候选GPIO与模块引脚、7B排针映射。
- `power-design.md`：供电预算、保护阈值、掉电路径及待验证项。
- `eda/`：可编辑原生工程；`exports/`：图纸、网表、布局及检查证据。文件缺失时不能视为相应阶段已完成。
- `eda/six-port-baseboard-v01.epro2`：当前3.2客户端原生工程；`.epro`为同时核对的兼容导出。两者均包含闭合板框和两个地铜区。已保存后重新打开PCB，核对对象保留情况。
- `check-netlist.py`、`check-pcb.py`、`check-native.py`：分别检查真实原理图网表、PCB读回数据、原生文件关键对象。检查通过不代表布线完成或实物通过。
- `footprint-fixes.md`：连接器孔径、S3地过孔及封装使用边界。

可在仓库根目录复核已保存快照：

```powershell
python hardware/wired_tire_central/baseboard/check-netlist.py hardware/wired_tire_central/baseboard/exports/six-port-v01.enet
python hardware/wired_tire_central/baseboard/check-pcb.py hardware/wired_tire_central/baseboard/exports/pcb-readback.json
python hardware/wired_tire_central/baseboard/check-native.py
```

## 已选架构

四个 ESP32-C3-MINI-1-H4X 分别承担四轮胎温采集与原有轮位BLE；一个 ESP32-S3-WROOM-1-N8 承担两路前刹车、报警和原有刹车BLE。每路各用THVD2410及TPS1H200A，通信线和探头电源均分支保护。S3为无PSRAM的8MB Flash型号；H4X模块的105°C规格不等于整机温度等级。

P4与五个从机共用短距离SPI。SN74HCS151选择五个DMA READY电平；三选择线加一个返回输入，使板间信号合计12根。四胎温热图仍是完整32×24，RaceChrono仍沿用既有四轮分区和刹车格式。

屏幕与底板通过SN74AXC8T245和SN74AXC4T245连接，A侧为底板3V3，B侧为 `P4_3V3_REF`。两条3V3不并联。P4供电参考只用于接口隔离器和本域上拉；底板3V3 PG通过开漏MOS使能接口。五个MCU本地各有Nexperia **74LVC125APW,118**，用于隔离MCU启动输出及MISO争用；禁止未审查地替换为TI SN74LVC125APWR，后者数据表未给出同样的Ioff保证。

每个MCU的ARM默认下拉，DE_CMD与ARM经与门后才可开启RS485发送；ARM也控制探头电源。四C3仅用原生USB维护，GPIO20 ARM不与UART0下载器TX相接。编程连接器不带USB5V，维护时底板需独立供电。

## 接口约定

六路探头接口暂定：1=本支路受保护电源、2=GND、3=A+、4=B−。这是**底板定义**，不等同于探头厂商插头或线色。连接器编号视图、压接端子、屏蔽线连接及实物插拔仍须核对。五路SPI、READY和ARM固件适配尚未完成。

7B使用P3和P1排针上的12个GPIO。P3-12接独立的 `P4_3V3_REF`；P1的3V3/VO4/BAT不接底板电源。7B的5V从单独电源连接器经过专用供电线束进入USB供电口，不能向信号排针送12V。屏幕USB与电脑同时连接时的VBUS路径需另做实板检查。

## 当前边界

旧固件、UUID、BLE设备名、轮位、RejsaRubberTrac及DIY CAN数据布局均未在本阶段修改；未启用任何Approved硬件profile。原离线C3固件不能直接烧录到新S3节点；READY选择、ARM上电顺序和新GPIO需要后续固件移植与台架验证。

7B及成品探头尚未到货；150×75mm板框、连接器位置和外壳开孔只能作为待装配核对的样机设计。制造前需完成布线、网络/封装/电源审查及真实DRC；上车前还需限流电源、分路故障、不同上电顺序、温升与车载瞬态测试。
