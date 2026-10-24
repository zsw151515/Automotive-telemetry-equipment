# 六口硬件放行记录

状态：NOT RUN；没有实物/手机版本/仪器记录，没有 Approved profiles，没有投板文件。

|关卡|所需证据|结果|
|---|---|---|
|实际7B板号/芯片修订|照片、esptool chip/rev输出、prev3/postv3选择依据|NOT RUN|
|13根SPI/CS/READY|逐针与原理图对照；34/36绑带上电波形；46–48 VO4实测电平；外设冲突|NOT RUN|
|第五C3 GPIO|模块原理图、双UART、USB18/19、Flash/绑带；两DE上电硬件关断|NOT RUN|
|5V及12V电源|限流电源、电流/压降、屏幕启动、接口反灌；后续车规浪涌与反接测试|NOT RUN|
|面板及触摸|EK79007双缓冲、背光32反相、RST33；GT911 RST23与NC差异核准|NOT RUN|

固定资料见7b_resource_review.md、7b_source_audit.md。合成validator脚号不得当接线表。只有以上逐项完成并签注板号/日期/操作者/测点后才能建立ApprovedBrakeProfile.h和ApprovedDisplayProfile.h。编译成功不得把approved置真。
