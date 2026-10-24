#include "Board7BTouch.h"
#include <cstring>
using namespace wired_brake;
esp_err_t Board7BTouch::read(esp_lcd_panel_io_t*io,int reg,void*out,size_t n){
 auto*self=reinterpret_cast<BoundIo*>(io)->owner;uint8_t address[]{uint8_t(reg>>8),uint8_t(reg)};
 const auto result=i2c_master_transmit_receive(self->device_,address,2,static_cast<uint8_t*>(out),n,10);
 if(result==ESP_OK){if(reg==0x814e&&n==1){self->status_=*static_cast<uint8_t*>(out);self->statusRead_=true;}if(reg==0x814f&&n<=40)std::memcpy(self->points_,out,n);}
 return result;
}
esp_err_t Board7BTouch::write(esp_lcd_panel_io_t*io,int reg,const void*data,size_t n){
 if(n>40)return ESP_ERR_INVALID_ARG;
 auto*self=reinterpret_cast<BoundIo*>(io)->owner;uint8_t buffer[42]{uint8_t(reg>>8),uint8_t(reg)};if(n)std::memcpy(buffer+2,data,n);return i2c_master_transmit(self->device_,buffer,n+2,10);
}
bool Board7BTouch::begin(const Board7BProfile&p){
 if(enabled_||!approved7B(p))return false;
 i2c_master_bus_config_t config{};config.i2c_port=I2C_NUM_0;config.sda_io_num=GPIO_NUM_7;config.scl_io_num=GPIO_NUM_8;config.clk_source=I2C_CLK_SRC_DEFAULT;config.glitch_ignore_cnt=7;config.flags.enable_internal_pullup=true;
 if(i2c_new_master_bus(&config,&bus_)!=ESP_OK)return false;
 uint8_t address=0;for(uint8_t a:{uint8_t(0x5d),uint8_t(0x14)})if(i2c_master_probe(bus_,a,10)==ESP_OK){address=a;break;}
 if(!address){i2c_del_master_bus(bus_);bus_=nullptr;return false;}
 i2c_device_config_t device{};device.dev_addr_length=I2C_ADDR_BIT_LEN_7;device.device_address=address;device.scl_speed_hz=400000;
 if(i2c_master_bus_add_device(bus_,&device,&device_)!=ESP_OK){i2c_del_master_bus(bus_);bus_=nullptr;return false;}
 io_.owner=this;io_.base.rx_param=read;io_.base.tx_param=write;io_.base.del=noDelete;
 esp_lcd_touch_config_t touch{};touch.x_max=1024;touch.y_max=600;touch.rst_gpio_num=GPIO_NUM_NC;touch.int_gpio_num=GPIO_NUM_NC;
 if(esp_lcd_touch_new_i2c_gt911(&io_.base,&touch,&touch_)!=ESP_OK){i2c_master_bus_rm_device(device_);i2c_del_master_bus(bus_);device_=nullptr;bus_=nullptr;return false;}
 enabled_=true;Serial.println("7B GT911 initialized SDA7/SCL8 NC reset/interrupt polling, I2C timeout10ms");return true;
}
TouchSample Board7BTouch::poll(uint64_t now){
 if(!enabled_)return {};
 if(now<next_)return cached_;
 next_=deadlineAfter(now,10000);statusRead_=false;
 if(esp_lcd_touch_read_data(touch_)!=ESP_OK){cached_={};return cached_;}
 if(statusRead_&&(status_&0x80)){
  TouchSample sample{};const size_t n=(status_&15)*8;
  if(!decodeGt911Report(status_,points_,n,sample)){cached_={};return cached_;}
  sample.sampledUs=now;cached_=sample;
 }
 return cached_; // Original local sampling time retained when GT911 has no new report.
}
