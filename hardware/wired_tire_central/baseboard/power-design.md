# 电源与掉电设计记录

本页是设计计算，非实测结果。输入取点火后12V，正常连续预算先按9–16V；冷启动跌落可触发欠压关机，不承诺不中断冷启动。车辆取电点附近安装外部5A保险，保险曲线、线径、接插件温升及满载实测仍需核对。

## 主保护

LM74900QRGERQ1配两只BSC040N10NS5（100V N-MOS），采用共漏极拓扑，中间2mΩ采样电阻。Q1源极朝输入，Q2源极朝输出。RTN裸焊盘悬空；VS与A近端各有100nF去耦，CAP至VS采用220nF/25V；SLEEP_OV接VS，采用过压切断。SLEEP与EN随主保护内部VS保持有效，整机开关由外部点火电源实现，不能用SLEEP的旁路模式代替断电。

| 项目 | 配置 | 标称计算 |
|---|---|---|
| UVLO | 107k/9.09k | 下降关断约7.02V，上升恢复约7.66V |
| OVP | 95.3k/3.32k | 上升关断17.823V，下降恢复16.338V |
| OCP | RSENSE=2mΩ，RSET=75Ω，RILIM=56.2k | 8.007A |
| 短路检测 | ISCP经1k接采样前端 | 约15.25A，具体误差与响应时间按手册复核 |
| OCP定时 | 68nF | 过流延时约0.99ms，自动重试约1.54s |
| IMON | 3.32k至GND | 0.07968V/A；仅作为测试网络，本阶段未接ADC |
| 启动斜率 | HGATE经1k与2.2nF/100V串联支路至GND | 不将RC接到OUT，不将1k串进主栅极驱动线 |

阈值均需计芯片和电阻误差。仅OV比较器参考误差就使上升阈值约17.377–18.714V，不能将17.823V标称值视为绝对上限。8A主保护用于板级故障识别，不替代5A线束保险。

原始输入采用双向SMCJ24CA，保护后母线采用双向SMBJ18CA并加负压Schottky钳位。双向TVS不承担反接截止，且其负向钳位远低于降压器负压限值，必须保留Schottky。SS54实际库符号**1=A、2=K**：D3阳极接地、阴极接保护母线；D4阳极接蜂鸣MOS漏极、阴极接5V。完整浪涌能量、布局寄生和MOS SOA尚未闭合，不能由TVS标称功率推导整机通过车载认证。

## 两路降压与屏幕

两只LMR33630BDDAR为HSOIC8工业型号，B=1.4MHz。各用2.2µH MPL-AL5030-2R2、输入2×10µF/50V加220nF，输出预留4×22µF/25V，BOOT-SW为100nF，VCC-GND为1µF。VCC是内部偏置，不能外接负载；AGND裸焊盘必须接地。Q1版为不同封装，不可直接替换。

5V反馈100k/24.9k，标称5.016V；3.3V反馈100k/43.2k，标称3.315V。5V反馈的芯片和1%电阻误差需与7B允许电压核对。电容DC偏压降容、纹波和密闭热环境仍须验证。电感原厂列Isat@100°C典型11A，不能写成125°C最小保证。

屏幕支路为LM74700QDBVRQ1+CSD17578Q5A理想二极管：源极接5V_BUCK、漏极接5V_SCREEN，VCAP至ANODE为100nF。其正向压降约20mV量级，不是零；无软启动和独立限流，EN拉低也不阻断正向体二极管。它用于阻断屏幕向底板反灌，不能据此保证7B向电脑USB VBUS不会反灌。

暂按屏幕5V/2A、底板3.3V/2.5A、六探头各12V/150mA进行正常预算：约29W负载功率；两路降压暂按90%效率、探头按恒功率保守估算时，9V输入约3.5A。此为预算假设，手册中的胎温探头30mA@6V不能当成12V最大电流；全部探头与屏幕启动峰值尚待测量。满额3A+3A和六支路限流同时取用会超过5A保险预算，不能承诺这些上限同时连续工作。

## 六路支路

TPS1H200AQDGNRQ1每路VS接受保护12V，OUT只供本探头；CL=6.19k，约323mA标称，含主要容差约256–392mA。DELAY经100k到VS，采用自动重试，典型40ms导通/1s关闭；不用持续限流模式长期耗散。FAULT六路线与上拉到逻辑3V3，送S3 GPIO1；低负载可报开路，不能把所有FAULT都显示成短路。

每路THVD2410供3V3、100nF去耦，RO经1k进入MCU；/RE和DE共同由DE_CMD AND ARM驱动。接口A/B各有SMBJ36CA到GND。可选120Ω终端及390Ω偏置初始DNP，先测成品探头自带终端，再根据线长和波形决定是否装配。4.5m线束需双绞差分线，并检查地电位差；本方案不是隔离式RS485。

## 板间电源域

两只AXC接口器件分别由底板3V3与P4_3V3_REF供电，任一轨掉到原厂规定的掉电条件时，输出被隔离；控制脚参考底板3V3。底板3V3 PG控制NMOS拉低默认上拉的/OE。五路CS两侧各自上拉到本域，绝不通过单侧通电缓冲加上拉电阻给另一个掉电母线反灌。P4复位时CS仍需保持高，共享MISO以2.2k下拉。最终需用示波器验证慢掉电和棕断斜坡，不能将Ioff在VCC=0的规格扩展为所有斜坡的已验证行为。

## 原厂依据

- [LM74900-Q1](https://www.ti.com/lit/ds/symlink/lm74900-q1.pdf)、[LMR33630](https://www.ti.com/lit/ds/symlink/lmr33630.pdf)、[TPS1H200A-Q1](https://www.ti.com/lit/ds/symlink/tps1h200a-q1.pdf)
- [LM74700-Q1](https://www.ti.com/lit/ds/symlink/lm74700-q1.pdf)、[CSD17578Q5A](https://www.ti.com/lit/ds/symlink/csd17578q5a.pdf)、[BSC040N10NS5](https://www.infineon.com/assets/row/public/documents/24/49/infineon-bsc040n10ns5-datasheet-en.pdf)
- [SN74AXC8T245](https://www.ti.com/lit/ds/symlink/sn74axc8t245.pdf)、[SN74AXC4T245](https://www.ti.com/lit/ds/symlink/sn74axc4t245.pdf)、[Nexperia 74LVC125A](https://assets.nexperia.com/documents/data-sheet/74LVC125A.pdf)
- [THVD2410](https://www.ti.com/lit/ds/symlink/thvd2410.pdf)、[MPL-AL5030-2R2](https://www.monolithicpower.com/en/mpl-al5030-2r2.html)

原厂数据用于初步设计；采购库中TVS/Schottky具体厂家、供应商封装及3D模型仍按实际物料单逐一复核，不能仅按通用料名互换。
