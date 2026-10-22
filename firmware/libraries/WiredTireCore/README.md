# WiredTireCore

四路有线胎温中控共享 C++17 核心。独立于 Arduino GPIO、串口与 BLE，允许在原生工具链验证协议和异常边界。

当前的 YF-RS485-640 全帧解析按手册 `0x65`、785 个寄存器、1570 字节寄存器数据实现。实物版本尚未验证，不能将合成帧测试解释为探头兼容性结论。

温度保留有符号 0.1°C 计数；像素有效性独立保存。错误响应不覆盖旧帧，接收有效响应也不等于确认探头完成新采样。首版只接受手册中的完整寄存器布局；其他布局必须抓包核对后扩展。

测试命令：`powershell -File scripts/test_wired_tire_core.ps1 -Case yf90640`。原生编译器可用 `-CompilerPath` 或 `WIRED_TIRE_CXX` 指定；支持 g++、clang++ 或 Zig 的 C++ 驱动。脚本找不到编译器或测试时必须失败。

Arduino 构建时通过 `--libraries firmware/libraries` 使用本库；Arduino IDE 交付安装方法由最终软件包提供。
