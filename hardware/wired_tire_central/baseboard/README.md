# 六接口底板 V0.1 工程样机

本目录对应嘉立创EDA专业版独立工程“六接口中控底板 V0.1 工程样机”。目标是同壳安装7B套件、四路胎温RS485、两路前刹车RS485及蜂鸣报警。图纸仍处工程设计阶段；是否完成网表、布局和审查，以 `review.md` 和实际导出文件为准，不能直接用于下单。

2026-10-26 阶段快照：12页原理图、286个器件、150×75mm四层PCB。原理图1044个已定义引脚网络、PCB1178个实体焊盘网络及器件位置已核对。当前API及`.epro`有4217个走线片段，`.epro2`另存两个同网极短片段，铜几何并集已核对；668个板上过孔及12个S3模块散热过孔。左前维护USB完整差分对已连接，保存重开后的真实DRC从22项降至16项，均属于其余四组USB，未报告其他类别错误。其余四路RC和差分布线、制造阻抗及实测尚未完成，不能投板。

## 工程资料

- `parts.json`：精确器件型号、厂家、立创编号、官方库UUID、符号与封装引用；不表示实时库存。
- `schematic-model.json`：各器件位号、图页、焊盘到网络映射及DNP配置。原理图坐标单位为10mil，PCB坐标单位为mil。
- `pinmap.csv`：本次候选GPIO与模块引脚、7B排针映射。
- `power-design.md`：供电预算、保护阈值、掉电路径及待验证项。
- `eda/`：可编辑原生工程；`exports/`：图纸、网表、布局及检查证据。文件缺失时不能视为相应阶段已完成。
- `eda/six-port-baseboard-v01.epro2`：当前3.2客户端原生工程；`.epro`为同时核对的兼容导出。两者均包含闭合板框和两个地铜区。已保存后重新打开PCB，核对对象保留情况。
- `check-netlist.py`、`check-pcb.py`、`check-native.py`：分别检查真实原理图网表、PCB读回数据、原生文件关键对象。检查通过不代表布线完成或实物通过。
- `check-mechanical.py`：用真实26个连接器焊盘坐标核对底面封装变换，避免后盖定位孔按错误镜像投影。
- `exports/protected-copper-baseline.json`：当前保护基线606条线、298个板过孔；模块内12孔另核对。明确的局部替换链见`protected-copper-revision.json`、`protected-copper-revision-3.json`与`protected-copper-revision-4.json`。
- `exports/power-routing-check.json`：08aaf0e电源支路检查点的写入批次、754段铜线/65个过孔增量及三处焊盘过孔坐标。
- `exports/local-rework-check.json`：59c861b检查点的局部调整、信号恢复、S3热焊盘接地及49项连接提示记录。
- `exports/left-sector-check.json`、`dense-vip-check.json`：后续左侧四元件重排、六引脚扇出、供电/地焊盘过孔和母线绕孔记录；最新数量以`pcb-drc-summary.json`为准。
- `footprint-fixes.md`：连接器孔径、S3地过孔及封装使用边界。

可在仓库根目录复核已保存快照：

```powershell
python hardware/wired_tire_central/baseboard/check-netlist.py hardware/wired_tire_central/baseboard/exports/six-port-v01.enet
python hardware/wired_tire_central/baseboard/check-pcb.py hardware/wired_tire_central/baseboard/exports/pcb-readback.json
python hardware/wired_tire_central/baseboard/check-native.py
python hardware/wired_tire_central/baseboard/check-mechanical.py
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

主电源使用单独的 `powerWide1oz` 导线规则：仅七个供电网络允许最大4mm宽度，其他间距、孔径和最小线宽没有放宽。原理图和PCB已同步该规则。普通信号布在顶层/底层，Inner1保留地平面、Inner2用于电源及本轮局部栅极控制走线；159个普通网络已加入布线，USB单独处理。当前最细走线约0.128mm，板上普通过孔为0.30/0.60mm；S3模块内散热孔为0.30/0.50mm。过孔在焊盘内的工艺仍需制造审查。

3V3主支路采用外层0.5mm、内层0.8mm，细间距器件的局部引出采用0.25mm。R17.1、R32.1和U25.7新增焊盘过孔，设计按树脂塞孔及铜盖电镀填平要求继续审查，普通盖油不能替代。该要求尚未转换为厂家确认的制造工艺；不作为普通盖油板下单。

本轮将C52沿Y上移0.8mm，替换三段局部电源走线并恢复两条受影响的信号，补齐主保护控制连接。R124.1至R119.1按独立Kelvin支线布置；C55.1新增焊盘过孔同样需要填孔及铜盖审查。S3三个热焊盘接地跨接在原生编辑器中按过孔位置分成六个片段。后续密集供电/地线试验未恢复全部信号，因此没有应用。

后续将R120、R121、R123和C56左移，U31左侧六引脚按0.2mm走线和0.30/0.60mm过孔完整扇出，电压检测及受影响的定时网络均恢复连接。R29.1、R31.1、U13.14、R35.2、C23.2新增五处焊盘过孔，也必须填孔及铜盖。3V3母线在两处地孔旁各用两条0.8mm并行线绕行，其他段保持3mm；未放宽间距规则。实际铜线并集与离线方案复核一致，原生DRC中3V3与GND均无连接错误。

7B及成品探头尚未到货；150×75mm板框、连接器位置和外壳开孔只能作为待装配核对的样机设计。制造前需完成布线、网络/封装/电源审查及真实DRC；上车前还需限流电源、分路故障、不同上电顺序、温升与车载瞬态测试。

本次SPI检查点将R3移至(23.3,58.0)并旋转至90°，R5移至(25.0,59.25)，引脚网络和值保持不变。两处SPI断点已闭合，所有受影响信号均恢复连接。3V3母线增加6.2mm长、2mm宽的局部缩颈，需在最终层叠和负载下核对温升。R4.2新增焊盘过孔，R5.1覆盖既有过孔，两处均要求填孔和铜盖。详细几何、替换范围和剩余27项连接提示见`exports/spi-checkpoint-check.json`和`protected-copper-revision-5.json`。本次未修改固件或RaceChrono数据定义。

12V钳位和右前输出检查点已补齐D2、D3、C54的三条内层分支及22个并联过孔，U26.7通过焊盘过孔引出并连接FR_VPROBE。全部新增焊盘过孔须填孔/铜盖；其中U26.7使用实际坐标(39.27513354,41.75)mm避让相邻铜线。未移动器件或更改引脚、数值、规则和固件。该供电检查点只剩五组USB差分对未布通；瞬态脉冲、负载温升、实物装配尚待验证。详见`exports/power-endpoints-check.json`及保护基线修订6。

历史RC预留检查点`eb1a2d4`仅完成左前路的33Ω串联电阻R134/R135及10pF DNP电容C76/C77预留和局部引出，当时五组完整USB差分对均未布通。该检查点增加四件后为286器件/1178实体焊盘，保留了此前282件的位置及3990段既有走线、659个含模块过孔的对象；记录见`exports/usb-fl-rc-check.json`。后续左前完整布线状态以下段及`exports/usb-fl-route-check.json`为准。

左前USB布线检查点：J11原地由0°转为180°，1～6脚仍为DM/DP/GND/EN/GND/BOOT，实体朝向已改变，维护线束须按针脚编号核对。两条顶层路径各约82.66465mm，无USB换层孔；实际重建的Inner1地覆盖差分主干。U26.3新增0.30/0.60mm焊盘过孔须填孔/铜盖。详见`usb-design.md`和`exports/usb-fl-route-check.json`。
