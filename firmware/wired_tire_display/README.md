# 微雪7B六口中控P4

默认离线构建，实际电气/芯片修订/触摸/帧率尚未验证。完整入口wired_tire_display.ino；五从机共享SPI owner，四热图与刹车/设置三页，切页不改变后台调度。第五C3自身采集/模型/蜂鸣/BLE不依赖P4。

Board7BProfile所有批准标志false、引脚-1。板上EK79007双lane1000Mbps、LDO3/2500mV、RGB565双缓冲、BL32反相5kHz/10bit、RST33；GT911 SDA7/SCL8、0x5D/0x14探测、NC复位/中断需实物核准，不驱动猜测GPIO23。13个板间信号涉及绑带和VO4电压域，未闭合硬件门槛。

src/vendor内固定官方EK79007 2.0.2~1、GT9111.1.3、touch1.1.2及许可证/逐文件哈希，只有头部前置一致的VendorConfig.h。只随本P4 sketch编译，不污染C3或纯核心。完整安装、协议及实物步骤见docs12/13和hardware/wired_tire_central/notes/。
