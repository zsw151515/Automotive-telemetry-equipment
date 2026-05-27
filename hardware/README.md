# hardware 硬件目录

本目录用于存放原理图、PCB、Gerber、BOM、3D 文件和硬件说明。

## 当前目录规划

```text
hardware/
├─ tire_temp_height_v2/                       # 历史参考
├─ tire_temp_height_v3/                       # 当前胎温高度主线
├─ brake_temp_12v_max31856/                   # 独立热电偶卡钳测温版本
├─ brake_temp_12v_rs485/                      # 初代RS485红外刹车盘版本，不带显示报警，非重点
└─ brake_temp_alarm_display_12v_rs485/        # 当前刹车节点板重点硬件
```

## 刹车节点硬件版本边界

```text
brake_temp_12v_max31856
= MAX31856 + K 型热电偶
= 测卡钳温度
= 当前未接入报警显示
= 独立采集 / 测试版本

brake_temp_12v_rs485
= RS485 红外测温传感器
= 测刹车盘温度
= 不带显示报警
= 初代版本，不是当前重点

brake_temp_alarm_display_12v_rs485
= RS485 红外测温传感器
= 测刹车盘温度
= 带显示、ALM、按键、蜂鸣器、RaceChrono
= 当前刹车节点板重点推进版本
```

## 放置规则

每个硬件版本建议包含：

```text
schematic/   原理图 PDF / EasyEDA 导出
pcb/         PCB 源文件或截图
gerber/      Gerber 压缩包
bom/         BOM / CPL / 贴片文件
3d/          STEP / 3D 文件
notes/       修改记录、测试记录、已知问题
README.md    版本说明
```

硬件已验证的修复结论必须写进 README 或 notes，不要只留在聊天记录里。
