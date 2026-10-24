#pragma once
#include <Arduino.h>
#include "Board7BProfile.h"
#include <WiredTireCore.h>
#include <driver/i2c_master.h>
#include <esp_lcd_panel_io_interface.h>
#include "src/vendor/esp_lcd_touch_gt911.h"
class Board7BTouch {
 public:WT_OFFLINE_LINK bool begin(const Board7BProfile&);WT_OFFLINE_LINK wired_brake::TouchSample poll(uint64_t);
 private:
  struct BoundIo{esp_lcd_panel_io_t base{};Board7BTouch*owner=nullptr;}io_;
  static esp_err_t read(esp_lcd_panel_io_t*,int,void*,size_t);
  static esp_err_t write(esp_lcd_panel_io_t*,int,const void*,size_t);
  static esp_err_t noDelete(esp_lcd_panel_io_t*){return ESP_OK;}
  i2c_master_bus_handle_t bus_=nullptr;i2c_master_dev_handle_t device_=nullptr;esp_lcd_touch_handle_t touch_=nullptr;
  uint8_t status_=0,points_[40]{};bool statusRead_=false,enabled_=false;uint64_t next_=0;wired_brake::TouchSample cached_{};
};
