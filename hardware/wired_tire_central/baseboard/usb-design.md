# 维护USB接口设计与当前实施范围

2026-10-26：五个MCU各自保留独立的USB维护排针，用于烧录和调试；不提供USB VBUS输入，维护时仍由外部12V供电。未更改任何固件、BLE或RaceChrono数据定义。

乐鑫的[C3原理图指南](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c3/schematic-checklist.html#usb)与[S3原理图指南](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32s3/schematic-checklist.html#usb)均建议USB引脚附近预留串联电阻（初值22/33Ω）和不装的对地调试电容。当前采用33Ω串联预留；10pF C0G是所选DNP器件资料中的名义值，不是已验证的调谐值，不得据此直接装配电容。

| 轮位/控制器 | MCU/维护排针 | 串联电阻 | DNP电容 | 当前状态 |
|---|---|---|---|---|
| 左前FL | U1/J11 | R134、R135，33Ω，C23140 | C76、C77，10pF C0G，C1634 | 原理图和PCB已加入；局部源端及接地已连接，完整差分对未完成 |
| 右前FR | U2/J12 | R136、R137 | C78、C79 | 仅预留位号，未加入原生工程 |
| 左后RL | U3/J13 | R138、R139 | C80、C81 | 仅预留位号，未加入原生工程 |
| 右后RR | U4/J14 | R140、R141 | C82、C83 | 仅预留位号，未加入原生工程，邻近器件布局须调整或重新选封装 |
| 刹车BRAKE | U5/J15 | R142、R143 | C84、C85 | 仅预留位号，未加入原生工程 |

`usb-interfaces.json`定义目标引脚和位号，不表示所有五路都已实现。当前阶段验证命令为：`python check-usb-interface.py --group FL`；不传分组选项会要求五路全部实现，当前不会通过。

## 左前实施定义

| 对象 | 原网络 | 当前网络/连接 | GPIO/管脚是否改变 |
|---|---|---|---|
| U1模块26脚，GPIO18 | FL_USB_DM | FL_USB_DM_MCU → R134.1，R134.2 → FL_USB_DM | 否 |
| U1模块27脚，GPIO19 | FL_USB_DP | FL_USB_DP_MCU → R135.1，R135.2 → FL_USB_DP | 否 |
| J11.1 / J11.2 | FL_USB_DM / FL_USB_DP | 保持原排针端网络；分别接电阻输出及C76.1/C77.1 | 否 |
| J11.3/4/5/6 | GND / FL_EN / GND / FL_BOOT | 保持 | 否 |
| C76.2 / C77.2 | 新增 | GND，局部0.25mm引出及0.30/0.60mm地孔 | 新增DNP预留 |

新增器件均在顶层：R134(28,65.4,0°)、R135(28,67,0°)、C76(30.5,64.5,270°)、C77(30.5,67.4,90°)。坐标单位mm。电容在原理图和PCB的加入BOM标记均为否，保留实体焊盘。

两段0.16mm局部MCU至电阻引出线长约1.51248mm和1.51244mm；该数值只描述局部引出，不能代替完整USB差分对的长度匹配。排针至电阻、含DNP焊盘的完整路径尚未布通。当前DRC为22项USB连接提示，无其他类别；相较上个20项检查点，多出两处未连接的电容信号焊盘。

## 后续布线与制造前检查

按乐鑫[USB布线指南](https://docs.espressif.com/projects/esp-hardware-design-guidelines/en/latest/esp32c3/pcb-layout-design.html#usb)，完整路径应成对、平行、等长，并具备连续参考地；目标差分阻抗90Ω±10%。优先顶层，参考相邻内层地。不得通过分别自动布通D+/D-就宣称差分布线完成。

板厂[JLCPCB层叠表](https://jlcpcb.com/impedance)中的JLC04161H-3313为当前核对候选：1.6mm四层、外层35µm、内层15.2µm、外层至地介质约0.09940mm。最终制造层叠、阻抗线宽/间距及验收方式尚未确定；0.16mm局部引出不构成已实现90Ω的证明。

完成完整路径后，还须核对：真实铜长度和间距、参考地连续性、必要的换层回流孔、封装钢网开窗、维护线束及USB枚举/烧录实测。当前不能投板。
