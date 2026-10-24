# 六口底板双前刹车 C3（离线方向）

完整入口为 wired_brake_channel.ino；两独立9600 8N1只读03/04探头（基线地址01/02），UART0/1、原生USB CDC日志、独立蜂鸣/NVS/BLE。P4失联不停止本机模型/报警/RaceChrono。FAIL本身静音，不把失联当正常。Core/Dose是估算，不是第三个实测探头。

默认无profile、引脚-1、全部I/O禁用。使用 `scripts/compile_wired_tire.ps1 -Target brake` 编译SDK完整路径；实际烧录前需要Reviewed ApprovedBrakeProfile.h、双UART与USB18/19、外部DE上电门控、电源/收发器/蜂鸣极性逐项实测。缺profile时hardware构建拒绝，不可用合成测试脚号接线。

新NVS namespace brake6p_v1，32字节CRC记录双slot；旧brake12v仅只读校验迁移，不清除。Flash暂停会记录最大值，不能宣称完全无阻塞。设置保存确认才生效、未知结果先读回；重复保存不重置模型/静音。

BLE N BrakeTemp / 1FF8；八个PID和所有字节布局、单位、缩放/计算定义见 [接口文档](../../docs/13_六接口中控板间与刹车接口.md)。本目录没有修改旧独立刹车固件；实际探头功能码/寄存器/字节序、手机连接、掉电保存、汽车可靠性尚待实测。
