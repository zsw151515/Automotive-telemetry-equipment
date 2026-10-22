#pragma once
#include <Arduino.h>
#include <WiredTireCore.h>
#include <esp_lcd_mipi_dsi.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_io.h>
#include <esp_ldo_regulator.h>
#include <atomic>
struct PanelCommand {
  int command = 0;
  const uint8_t* data = nullptr;
  size_t size = 0;
  uint32_t waitMs = 0;
};
struct PanelConfig {
  bool approved = false, phyPoweredExternally = false;
  int phyLdoChannel = -1, backlightPin = -1, resetPin = -1;
  bool backlightActiveHigh = true, resetActiveLow = true;
  uint32_t backlightPwmHz = 0, resetPulseMs = 0, resetReleaseMs = 0;
  uint8_t brightness = 180;
  esp_lcd_dsi_bus_config_t bus{};
  esp_lcd_dpi_panel_config_t dpi{};
  const PanelCommand* commands = nullptr;
  size_t commandCount = 0;
};
class PanelDriver {
 public:
  WT_OFFLINE_LINK bool begin(const PanelConfig&);
  WT_OFFLINE_LINK uint16_t* drawBuffer();
  WT_OFFLINE_LINK bool present(const uint16_t* rgb565, size_t count);
  void setBrightness(uint8_t value);
  bool ready() const { return ready_; }
  uint32_t refreshes() const { return refreshes_.load(); }
 private:
  static bool IRAM_ATTR refreshed(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t*, void*);
  void cleanup();
  esp_lcd_dsi_bus_handle_t bus_ = nullptr;
  esp_lcd_panel_io_handle_t io_ = nullptr;
  esp_lcd_panel_handle_t panel_ = nullptr;
  esp_ldo_channel_handle_t ldo_ = nullptr;
  uint16_t* buffers_[2]{};
  std::atomic<uint32_t> refreshes_{0};
  wired_tire::DisplaySwapFence fence_;
  int backlightPin_ = -1;
  uint8_t targetBrightness_ = 0;
  bool presented_ = false, lightStarted_ = false;
  bool backlightActiveHigh_ = true, pwm_ = false, ready_ = false;
};
