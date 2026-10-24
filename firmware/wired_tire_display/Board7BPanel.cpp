#include "Board7BPanel.h"
#include "src/vendor/esp_lcd_ek79007.h"
bool Board7BPanel::begin(const Board7BProfile&p){
 if(ready_||!approved7B(p))return false;
 backlightPin_=32;backlightActiveHigh_=false;pinMode(32,OUTPUT);digitalWrite(32,HIGH);
 esp_ldo_channel_config_t ldo{};ldo.chan_id=3;ldo.voltage_mv=2500;
 if(esp_ldo_acquire_channel(&ldo,&ldo_)!=ESP_OK){cleanup();return false;}
 esp_lcd_dsi_bus_config_t bus{};bus.bus_id=0;bus.num_data_lanes=2;bus.phy_clk_src=static_cast<decltype(bus.phy_clk_src)>(0);bus.lane_bit_rate_mbps=1000;
 if(esp_lcd_new_dsi_bus(&bus,&bus_)!=ESP_OK){cleanup();return false;}
 esp_lcd_dbi_io_config_t io=EK79007_PANEL_IO_DBI_CONFIG();if(esp_lcd_new_panel_io_dbi(bus_,&io,&io_)!=ESP_OK){cleanup();return false;}
 esp_lcd_dpi_panel_config_t dpi=EK79007_1024_600_PANEL_60HZ_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB565);dpi.num_fbs=2;dpi.flags.use_dma2d=false;
 ek79007_vendor_config_t vendor{};vendor.mipi_config.dsi_bus=bus_;vendor.mipi_config.dpi_config=&dpi;vendor.mipi_config.lane_num=2;
 esp_lcd_panel_dev_config_t dev{};dev.reset_gpio_num=33;dev.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB;dev.bits_per_pixel=16;dev.vendor_config=&vendor;
 if(esp_lcd_new_panel_ek79007(io_,&dev,&panel_)!=ESP_OK||esp_lcd_panel_reset(panel_)!=ESP_OK){cleanup();return false;}
 const bool ok=adopt7B(p.brightness);if(ok)Serial.println("7B EK79007 initialized / two RGB565 buffers / first-frame backlight fence");return ok;
}
