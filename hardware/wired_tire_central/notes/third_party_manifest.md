# 第三方依赖锁定

仅移植显示底层驱动，不导入完整BSP/LVGL/音频/网络。上游来源与文件哈希由本次离线交付记录；许可证随 `firmware/wired_tire_display/src/vendor/` 源文件分发。

|依赖|固定来源|范围|
|---|---|---|
|Arduino-ESP32|本机3.3.7，IDF5.5.2|C3/P4 SDK，未升级全局环境|
|Waveshare BSP|3.0.1，d7567970be6d6a94077727eb69d580d63a898eab|仅对照板级配置|
|Waveshare示例|9fcd49a8c927a00f5583c22d568b3448addbe51c|仅对照，不使用整套示例|
|EK79007|2.0.2~1，esp-iot-solution 12f6ca1182ec48889b17ec570fadaaf267cb336e|MIPI DSI面板底层|
|GT911|1.1.3，esp-bsp 08b53c32b53d2b4c3c4ecbf2cb6f1fc91a0441fd|触控底层|
|esp_lcd_touch|1.1.2，同上esp-bsp提交|触控基础接口|

源文件逐项SHA256和许可清单将在接入任务中与实际入包文件核对补齐。当前尚未据此启用实物profile。

## 已下载源码与许可证 SHA256

- `esp_lcd_ek79007-2.0.2_1/esp_lcd_ek79007/esp_lcd_ek79007.c`：`0bd6882cbf9fc1a7dc932b288e99784519816d9d9989f15578abbfa094c76c1b`
- `esp_lcd_ek79007-2.0.2_1/esp_lcd_ek79007/include/esp_lcd_ek79007.h`：`d015530ae1ab69b1b9d11ce27a5570840a59bb203a424ab49da6788f25bba371`
- `esp_lcd_ek79007-2.0.2_1/esp_lcd_ek79007/license.txt`：`cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`
- `esp_lcd_touch-1.1.2/esp_lcd_touch/esp_lcd_touch.c`：`804eeea836e75a8a9c815fb53932a7d3c8811f2b167c3707f55bd400d376328b`
- `esp_lcd_touch-1.1.2/esp_lcd_touch/include/esp_lcd_touch.h`：`3769c5e0bb60a05475ec182607ca4edb02add622d38dd6ae75592ac0d8477cb1`
- `esp_lcd_touch-1.1.2/esp_lcd_touch/license.txt`：`cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`
- `esp_lcd_touch_gt911-1.1.3/esp_lcd_touch_gt911/esp_lcd_touch_gt911.c`：`84935addfa5e6d021d7ea464077500e6e41dab421952832b4b1b7a6242199a71`
- `esp_lcd_touch_gt911-1.1.3/esp_lcd_touch_gt911/include/esp_lcd_touch_gt911.h`：`28e56b5d7728edf76b7993b5d52921dea49e9508710b1111caaaab4e75585c5d`
- `esp_lcd_touch_gt911-1.1.3/esp_lcd_touch_gt911/license.txt`：`cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`
