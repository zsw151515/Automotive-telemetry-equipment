# 07 Git 与 Codex 协作流程

## 1. 推荐工作方式

本项目建议使用 GitHub 作为唯一可信代码库。

ChatGPT / Codex / 本地 Arduino IDE 都围绕这个仓库工作，避免代码散落在聊天记录、桌面文件和多个压缩包中。

## 2. 基本流程

推荐流程：

```text
1. 本地或网页上传当前稳定代码
2. 提交到 GitHub
3. 让 Codex 阅读 CODEX.md / AGENTS.md / docs
4. 让 Codex 修改指定文件
5. 审查改动
6. 编译和烧录
7. 实物测试
8. 测试通过后合并或打 tag
```

## 3. 给 Codex 的推荐指令

### 3.1 修改胎温高度节点 v3

```text
请先阅读 CODEX.md、AGENTS.md 和 docs/02_硬件版本说明.md。
然后修改 firmware/tire_temp_height_node_v3/ 下的固件。
不要修改引脚定义。
不要破坏 RaceChrono BLE 兼容性。
请输出完整 .ino 文件，并说明修改点和测试步骤。
```

### 3.2 修改刹车温度节点

```text
请先阅读 CODEX.md、AGENTS.md、docs/05_已知硬件问题与排查记录.md。
然后检查 brake_temp_12v_max31856 固件。
重点保留 MAX31856 fault 日志和异常处理。
不要简单把异常温度显示为 0。
请给出完整终版代码。
```

### 3.3 修改中控显示

```text
请先阅读 CODEX.md 和 docs/06_测试与交付检查清单.md。
修改中控显示界面，优先保证车内可读性。
小屏幕上不要堆太多状态文字。
保留报警阈值和按键逻辑。
请输出完整 .ino 文件。
```

## 4. 分支建议

建议：

```text
main                 稳定主分支
dev                  日常开发
feature/xxx          新功能
fix/xxx              修复问题
hardware/xxx         硬件文档更新
```

如果当前项目还在早期，可以先只使用 `main`，但重要稳定版本应打 tag。

## 5. Tag 建议

稳定版本建议打 tag：

```text
v2-node-stable-espnow-ble
v3-power-fix-ltc2954-2
v3-racechrono-ble-stable
brake-12v-max31856-bench-stable
central-display-oled-alarm-v1
```

## 6. Commit 信息建议

提交信息示例：

```text
docs: 初始化中文项目文档
firmware: 添加 v3 胎温高度节点 BLE 稳定版
fix: 修复刹车温度异常值处理
hardware: 记录 v3 电源控制修复方案
ui: 放大 OLED 主温度显示
```

## 7. 上传固件建议

上传固件时，每个目录建议包含：

```text
firmware/tire_temp_height_node_v3/
├─ tire_temp_height_node_v3.ino
└─ README.md
```

README.md 里写：

```text
硬件版本：
MCU：
传感器：
通信方式：
引脚表：
当前状态：实车稳定 / 台架稳定 / 开发中
烧录设置：
已知问题：
```

## 8. 上传硬件建议

上传硬件文件时建议：

```text
hardware/tire_temp_height_v3/
├─ schematic/
├─ gerber/
├─ bom/
├─ 3d/
└─ README.md
```

README.md 中记录：

- 原理图版本
- PCB 版本
- BOM 版本
- 已验证修复
- 不建议改动的器件
- 打板注意事项

## 9. Codex 审查重点

让 Codex 审查代码时，可以这样问：

```text
请从真实硬件稳定性的角度审查这份固件。
重点检查：
1. 是否会阻塞 BLE
2. 是否会导致传感器初始化失败后卡死
3. 是否保留故障日志
4. 是否有 NaN / 异常值处理
5. 是否可能误触发关机
6. 是否破坏 RaceChrono 兼容性
```

## 10. 不建议的流程

不建议：

- 只把代码贴在聊天里，不进 GitHub
- 每次用不同文件名但不记录版本
- 让 Codex 在不了解硬件的情况下重构
- 稳定版被覆盖但没有备份
- 只测试编译，不测实物

## 11. 当前仓库下一步建议

1. 把已有 `.ino` 稳定代码放入 `firmware/`。
2. 把 v2 / v3 原理图、Gerber、BOM 放入 `hardware/`。
3. 每个硬件版本增加一个 `README.md`。
4. 每个固件版本增加一个 `README.md`。
5. 给已经实测稳定的版本打 tag。
