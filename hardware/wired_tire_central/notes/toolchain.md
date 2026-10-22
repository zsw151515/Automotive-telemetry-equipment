# 离线工具链

- Arduino CLI 1.5.0；Arduino-ESP32 3.3.7，保留既有项目版本。
- 原生测试：Zig 0.14.1 的 C++17 驱动，来自 ziglang.org 官方 Windows x86_64 包，SHA256 为 554f5378228923ffd558eac35e21af020c73789d87afeabf4bfd16f2e6feed2c。
- 原生编译器存放在本计划忽略的工作目录；不作为产品依赖提交。
- Python 3.12。离线抓包测试仅用标准库；真实串口抓包需要 pyserial 3.5，目前未连接探头，也不自动访问任何端口。

原生测试可用 -CompilerPath 或 WIRED_TIRE_CXX 指定 g++、clang++、Zig。固件构建与原生测试结果应分别记录。
