#pragma once
// Arduino equivalent of upstream component build definitions. No LVGL/BSP dependency.
#define ESP_LCD_EK79007_VER_MAJOR 2
#define ESP_LCD_EK79007_VER_MINOR 0
#define ESP_LCD_EK79007_VER_PATCH 2
#ifndef CONFIG_ESP_LCD_TOUCH_MAX_POINTS
#define CONFIG_ESP_LCD_TOUCH_MAX_POINTS 5
#endif
#ifndef CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS
#define CONFIG_ESP_LCD_TOUCH_MAX_BUTTONS 0
#endif
