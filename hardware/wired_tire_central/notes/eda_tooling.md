# 嘉立创 EDA 专业版自动化接入记录

日期：2026-10-24。用户已选择嘉立创 EDA 专业版交付，并要求查找可操作原理图与PCB的技能、插件、MCP等工具。原六接口底板任务继续；本文记录接入和实际API兼容性；设计阶段以 `../baseboard/review.md` 及导出文件为准。

## 查到的可用路径

| 工具 | 能力及本次用途 | 状态 |
|---|---|---|
| 官方 [easyeda-api 技能](https://github.com/easyeda/easyeda-api-skill) | 提供原理图、PCB、封装/符号、工程管理API文档，以及执行API的本地桥接服务 | 已下载到本任务忽略目录，已读取技能与桥接代码 |
| 官方 [Run API Gateway](https://github.com/easyeda/eext-run-api-gateway) | 安装在EDA内，连接本地桥接并执行扩展API；所下载扩展清单要求 `eda: ~3.2.0`，与本机3.2.149系列匹配 | 用户已导入并开启外部交互；本机3.2.149已实际连接并执行工程读写 |
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
- 首次健康检查：`service=easyeda-bridge`、`status=ok`、`edaConnected=false`、`edaWindowCount=0`。**服务启动不等于EDA已经连接**。随后用户导入扩展，已确认 `edaConnected=true`、单一EDA窗口，并建立下述独立工程。
- 本次未修改既有工程、固件、引脚profile或RaceChrono协议，未产生已核准制造文件。

技能副本、日志、PID和依赖位于忽略目录；本机可导入扩展位于本工作树 `output/wired-tire/integrations/run-api-gateway_v1.0.5_zh-cn.eext`。这些本机文件不作为仓库源码或可复现硬件设计提交。

官方扩展发布地址：[v1.0.5](https://github.com/easyeda/eext-run-api-gateway/releases/tag/v1.0.5)。所下载中文包SHA256：`2a97471b76cd274eb1151559949d47ad8940a57b3044c4291eeb22e0935e196a`，已与官方GitHub发布资产digest核对。

## 客户端接入步骤

在嘉立创EDA专业版打开“高级 → 扩展管理器 → 导入”，选择上述 `.eext`；勾选“允许外部交互”和“显示在顶部菜单”，再从“API Gateway → Reconnect”连接。此步骤需要在客户端界面完成；不会通过修改安装目录或内部权限数据库代替勾选。

接通后先做只读检查：读取当前EDA版本、已连接窗口和当前工程。后续新建六口底板独立工程，避免覆盖现有工程；每次写入前核对目标工程和文档类型。原理图与PCB坐标单位不同（10mil与mil），放置后必须读回引脚/焊盘坐标并复核。规则检查、网表、预览及导出结果分别留证，不能把API调用成功写成电路正确或实物验证通过。

原理图、PCB、库和导出API能力见[官方API参考](https://prodocs.lceda.cn/cn/api/reference/pro-api.html)。连接能力与元器件选型、引脚核准、电源/射频/PCB工程审查是不同关卡；后者仍按用户指定顺序推进。


## 实际接入与3.2 API核对

2026-10-24开始接入，2026-10-25继续绘图。已建立独立工程“六接口中控底板 V0.1 工程样机”，工程UUID `cabf59e4c0fc400bb9f26d8801fd78d4`；同板原理图 `c894fbda5b2e2552`、PCB `f6a2934be26f0c80`。未覆盖用户已有工程。

- 当前为客户端在线模式，`getProjectsPaths()`在此模式不可用；不切换用户模式。导出File/Blob经桥接返回后，保存到本工作树。
- 使用本机 `api-types.d.ts` 与实现核对精确签名，不能直接套用最新API文档。3.2没有公开的PCB自动布局/自动布线API。
- `openDocument()`之后显式调用 `activateDocument(tabId)`，每批操作和各次写入前核对文档类型、图页UUID与工程UUID；仅打开标签页不足以替代激活核查。
- `sch_Net.getAllNets()`等接口在本版为空实现，不能用空结果证明网表无误。
- `sch_Netlist.getNetlist()`在实测中超时；改用已实际验证的 `sch_ManufactureData.getNetlistFile("明确文件名","JLCEDA")` 导出真实网表，避免旧入口逐封装补标题的串行请求。
- 原理图完整PDF使用 `getExportDocumentFile(...,"PDF",...)`；本版没有原理图 `getPdfFile()`。已验证初始完整PDF导出，最终图纸另存于底板目录。
- `sch_PrimitiveComponent.modify()`只允许普通器件；NetPort直接用 `createNetPort()`及 `getState_Net()`读回。跨页连接保留真实NetPort，并从导出网表确认，不假定普通同名短线总是跨页互通。
- 通过API查询的器件按精确厂家型号筛选，不能只取搜索第一条。逗号型号查询会返回无关器件；每只元件读回引脚号、坐标和封装引用。
- NC修改通过画布引脚 `setState_NoConnected()`，不编辑共享符号库；裸焊盘悬空与接地按具体芯片手册分别核对。

实际设计包含供电隔离、启动毛刺、保护二极管极性及热焊盘复核；API调用成功只证明操作完成，网表断言、规则检查和实物验证另行记录。

## 保存与PCB接口的实测注意事项

- `sch_PrimitiveComponent.modify()`的几何修改会重置部分器件属性。修改后须按实际位号恢复Supplier Part、Value、DNP及说明，并验证Unique ID和网表。直接修改属性对象可避免再次覆盖几何。
- 导线读回可能归一化端点次序；标注位置应从实际符号引脚识别外端，不能总把数组第二端当外端。NetPort的默认文字位置也需检查PDF。接口页以带名导线实现连接后，已从完整导出网表核对跨页连接。
- PCB器件旋转使用度，实体焊盘读回旋转在本版为弧度；重开后的270°可能表示为−90°。检查应比较等价角度，不能因此移动器件。
- 板框用一个闭合的 `pcb_PrimitivePolyline`，并用真实DSN导出验证。`pcb_PrimitiveLine`放在11层会形成普通走线，不能据视觉闭合推断导出器能识别。
- 覆铜优先级为所在铜层中的顺序，不能传任意大数。实测传10会在运行时建对象后、提交保存前报错。遇到报错先读回对象，不能直接重复创建。使用默认顺序修复后，已验证原生文件包含边界和填充，重开仍保留。
- 封装内过孔尺寸须在本工程本地封装修正；实例修改可改变网名，但不能代替封装几何更新。过孔读回孔径可能按mil取整，应核对原生源码。
- 当前同时保存`.epro2`与兼容`.epro`，并对归档内部的器件、闭合板框和铜区计数。`.epro2`可保留已无活动父边界的旧填充缓存；检查活动铜区引用，并以重开后的画布对象为准。

信号候选使用官方Freerouting 2.5.0 JAR及任务目录内的Temurin 25 JRE。Windows便携原生CLI在本工程出现`ObstacleArea`及`java.lang.Number`序列化错误，未采用其失败结果。JAR的SHA256为 `f6f51bb02245e8e717f9359bd260cc9c5c0b1bc0acc8b7cb2cd5b8ffeb5de3c7`，JRE压缩包为 `4c95451cea98556def2c54f7782933f52a26d4a36bd85e1d59f0364464828b07`；未全局安装Java。软件退出成功不替代回导后的真实DRC。[官方发行版](https://github.com/freerouting/freerouting/releases/tag/v2.5.0)、[参数说明](https://github.com/freerouting/freerouting/blob/v2.5.0/docs/settings.md)。

本轮只加入159个普通信号网络，电源、USB和敏感电源控制线排除；顶层/底层参与信号布线，Inner1和Inner2禁用信号自动布线。原生DSN需规范化数字走线层名、单引号网络名和旧间距类型；作用域名与标识符保持同一行，避免当前解析器的NAME状态读错换行。规则文件设计名须与输入文件名一致。导出的RF禁布对象被表示为越界虚拟焊盘，交换文件用实际RF禁布区替换；同时补上安装孔和连接器定位孔约束。

返回结果先按实际焊盘、已有铜线、孔边、RF区及最小线宽检查，再通过受保护的API任务写入；未直接覆盖整板走线。自动缩颈候选低于0.127mm时需要增宽并复查。已有电源/地线基线随工程保存，原生检查会核对它们是否保留。导入器的74条预存警告来自交换格式/库几何表示；最终依据嘉立创EDA原生DRC。本轮保存重开后仅有234项未连接提示，仍不具备制造条件。

3.2客户端的连接图可能在批量画线后保留旧缓存，应保存并重新打开PCB，再取得最终连接数量。底面连接器的局部变换为X保留、Y取反，已用26个真实焊盘验证；不能按X镜像推导定位孔或外壳中心。

原理图`Wire.create`可能自动合并相接导线并返回已有ID；`Wire.modify(net='')`会重命名合并后的整个网络，不能用来隐藏文字。图纸整理后必须重新核对完整网表；本轮接口图页移动保留原ID及属性，最终282器件/1036已定义引脚网络复核通过。
