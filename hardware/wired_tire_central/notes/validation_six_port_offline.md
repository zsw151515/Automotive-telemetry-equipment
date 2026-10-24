# 六接口离线验证记录 0.2.0

范围：四胎温C3＋双前刹车第五C3＋7B P4三页UI；hardware_validated=false、actual_fps_measured=false。真实探头/手机/汽车及GPIO核准见resource_approval.md和各validation文件，全部NOT RUN。

工具链：Arduino CLI1.5.0，ESP32 core3.3.7 / IDF5.5.2；原生Zig0.14.1 C++17、-Wall/-Wextra/-Wpedantic/-Werror；Python3.12。没有安装或升级全局SDK。Windows权限环境变更后，现有编译器需运行在允许MSYS/DLL的环境中；临时和构建文件在工作树，未更改系统执行策略。

离线覆盖：20ms双路Modbus期限/噪声隔离、0/负温/量程、V1.6模型/64位时间/长停顿、蜂鸣/60秒静音、双槽CRC/损坏槽恢复/Unknown、配置/会话/缓存重放、256字节BrakeLink与独立年龄、八PID/过滤、五从机DMA租约、命令超时停止变更重发、三页全像素/镜像/越界/触摸长按/回执状态及GT911原始报告、未核准profile前拒绝IO。

本次原生测试实际105个命名测试、22组全部通过；Python实际14项通过（含旧包不覆写）。prev3/postv3两变体均显式PSRAM=enabled且已编译/链接，刹车显式CDCOnBoot=cdc。最终交付需在源码提交后从ZIP重新解压，逐文件SHA256校验，再运行原生/Python及channel/brake/display两变体矩阵；具体最终结果补于本文件末尾。

已有0.1包和59项原生/10项工具结果只属于历史四轮包，不充当本次新包结果。预览为SIMULATED DATA，已检查三页1024×600布局，不是实屏或实测FPS。
