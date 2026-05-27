# hardware 硬件目录

本目录用于存放原理图、PCB、Gerber、BOM、3D 文件和硬件说明。

## 目录规划

```text
hardware/
├─ tire_temp_height_v2/
├─ tire_temp_height_v3/
└─ brake_temp_12v_rs485/
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
