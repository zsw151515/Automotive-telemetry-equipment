# 嘉立创 EDA 专业版自动化接入记录

日期：2026-10-24。用户已选择嘉立创 EDA 专业版交付，并要求查找可操作原理图与PCB的技能、插件、MCP等工具。原六接口底板任务继续；本文记录接入准备，不代表已生成电路或完成PCB布局。

## 查到的可用路径

| 工具 | 能力及本次用途 | 状态 |
|---|---|---|
| 官方 [easyeda-api 技能](https://github.com/easyeda/easyeda-api-skill) | 提供原理图、PCB、封装/符号、工程管理API文档，以及执行API的本地桥接服务 | 已下载到本任务忽略目录，已读取技能与桥接代码 |
| 官方 [Run API Gateway](https://github.com/easyeda/eext-run-api-gateway) | 安装在EDA内，连接本地桥接并执行扩展API；所下载扩展清单要求 `eda: ~3.2.0`，与本机3.2.149系列匹配 | 已准备中文官方安装包；尚待在EDA中导入并开启外部交互，实际兼容性待连接验证 |
| 官方 [CLI/MCP](https://github.com/easyeda/easyeda-client-cli) | 新版客户端内置命令行和MCP，可管理会话、执行API、查询文档及导出文件 | 官方要求4.1.60以上；本机3.2.149不包含此入口，本次未改客户端或Codex MCP配置 |
| 社区 [pcb-skill](https://github.com/daishuge/pcb-skill) | 面向选型、布局、布线和制造文件复核的工作流程参考 | 已查阅介绍，未安装、未执行其脚本；本次优先官方桥接 |

当前插件目录检索 `EasyEDA / 嘉立创 / LCEDA / PCB / circuit` 未返回匹配插件；该检索不代表所有第三方项目均不存在。直接使用官方API桥接即可开展本任务，不要求先安装额外社区MCP。

官方CLI仓库中的[兼容性问题记录](https://github.com/easyeda/easyeda-client-cli/issues/1)有维护者说明4.1.71修复早期MCP入口问题；如后续切换原生MCP，应选择已修复的新版本并实际测试，不以最低4.1.60版本要求等同于MCP已可用。

## 已核对的本机状态

- 已安装程序：`E:/lceda-pro/lceda-pro.exe`，`resources/app/package.json` 版本 `3.2.149.88089769`。
- Node.js：本机现有运行时 `v24.22.1`，未安装或升级全局Node/npm。
- 官方技能：`easyeda-api 1.1.60`，存放于本工作树 `.superpowers/sdd/2026-10-24-six-port-baseboard-hardware/skills/easyeda-api/`。这是任务本地副本，未注册为Codex全局技能。
- 桥接依赖：按上游lockfile取得 `ws 8.21.3`，校验SHA512完整性后放入该副本的 `node_modules/ws/`，未执行npm安装脚本。
- 本地桥接：已在后台启动，仅监听 `127.0.0.1:49620`；源码 `LISTEN_HOST='127.0.0.1'` 已核对。
- 首次健康检查：`service=easyeda-bridge`、`status=ok`、`edaConnected=false`、`edaWindowCount=0`。**服务启动不等于EDA已经连接**，尚未执行任何原理图/PCB读写。
- 本次未修改既有工程、固件、引脚profile或RaceChrono协议，未产生已核准制造文件。

技能副本、日志、PID和依赖位于忽略目录；本机可导入扩展位于本工作树 `output/wired-tire/integrations/run-api-gateway_v1.0.5_zh-cn.eext`。这些本机文件不作为仓库源码或可复现硬件设计提交。

官方扩展发布地址：[v1.0.5](https://github.com/easyeda/eext-run-api-gateway/releases/tag/v1.0.5)。所下载中文包SHA256：`2a97471b76cd274eb1151559949d47ad8940a57b3044c4291eeb22e0935e196a`，已与官方GitHub发布资产digest核对。

## 客户端接入步骤

在嘉立创EDA专业版打开“高级 → 扩展管理器 → 导入”，选择上述 `.eext`；勾选“允许外部交互”和“显示在顶部菜单”，再从“API Gateway → Reconnect”连接。此步骤需要在客户端界面完成；不会通过修改安装目录或内部权限数据库代替勾选。

接通后先做只读检查：读取当前EDA版本、已连接窗口和当前工程。后续新建六口底板独立工程，避免覆盖现有工程；每次写入前核对目标工程和文档类型。原理图与PCB坐标单位不同（10mil与mil），放置后必须读回引脚/焊盘坐标并复核。规则检查、网表、预览及导出结果分别留证，不能把API调用成功写成电路正确或实物验证通过。

原理图、PCB、库和导出API能力见[官方API参考](https://prodocs.lceda.cn/cn/api/reference/pro-api.html)。连接能力与元器件选型、引脚核准、电源/射频/PCB工程审查是不同关卡；后者仍按用户指定顺序推进。
