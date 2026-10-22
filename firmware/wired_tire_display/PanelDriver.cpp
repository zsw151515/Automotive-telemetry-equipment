#include "PanelDriver.h"
#include <driver/gpio.h>
#include <cmath>
#include <cstring>
bool IRAM_ATTR PanelDriver::refreshed(esp_lcd_panel_handle_t, esp_lcd_dpi_panel_event_data_t*, void* context) {
  static_cast<PanelDriver*>(context)->refreshes_.fetch_add(1, std::memory_order_relaxed);
  return false;
}
void PanelDriver::setBrightness(uint8_t value) {
  if (pwm_) ledcWrite(backlightPin_, backlightActiveHigh_ ? value : 255 - value);
}
void PanelDriver::cleanup() {
  ready_ = false;
  if (pwm_) { setBrightness(0); ledcDetach(backlightPin_); pwm_ = false; }
  if (backlightPin_ >= 0) { pinMode(backlightPin_, OUTPUT); digitalWrite(backlightPin_, backlightActiveHigh_ ? LOW : HIGH); }
  if (panel_) { esp_lcd_panel_del(panel_); panel_ = nullptr; }
  buffers_[0] = buffers_[1] = nullptr;
  if (io_) { esp_lcd_panel_io_del(io_); io_ = nullptr; }
  if (bus_) { esp_lcd_del_dsi_bus(bus_); bus_ = nullptr; }
  if (ldo_) { esp_ldo_release_channel(ldo_); ldo_ = nullptr; }
}
bool PanelDriver::begin(const PanelConfig& config) {
  if (ready_ || !config.approved || config.dpi.video_timing.h_size != 1024 ||
      config.dpi.video_timing.v_size != 600 || !GPIO_IS_VALID_OUTPUT_GPIO(config.backlightPin) ||
      !config.backlightPwmHz || (config.resetPin >= 0 && (!GPIO_IS_VALID_OUTPUT_GPIO(config.resetPin) ||
      config.resetPin == config.backlightPin || !config.resetPulseMs)) ||
      config.bus.bus_id != 0 || config.bus.num_data_lanes < 1 || config.bus.num_data_lanes > 2 ||
      !std::isfinite(config.bus.lane_bit_rate_mbps) || config.bus.lane_bit_rate_mbps <= 0 ||
      !std::isfinite(config.dpi.dpi_clock_freq_mhz) || config.dpi.dpi_clock_freq_mhz <= 0 ||
      (config.phyLdoChannel < 0 && !config.phyPoweredExternally) ||
      (config.phyLdoChannel >= 0 && config.phyPoweredExternally) ||
      config.commandCount > 256 || config.resetPulseMs > 500 || config.resetReleaseMs > 1000 ||
      (config.commandCount && !config.commands)) {
    Serial.println("Panel not initialized: reviewed panel timings/pins/power/commands required");
    return false;
  }
  uint64_t totalWaitMs = uint64_t{config.resetPulseMs} + config.resetReleaseMs;
  for (size_t i = 0; i < config.commandCount; ++i) {
    totalWaitMs += config.commands[i].waitMs;
    if (config.commands[i].waitMs > 1000 || totalWaitMs > 10000 ||
        (config.commands[i].size && !config.commands[i].data)) return false;
  }
  backlightPin_ = config.backlightPin; backlightActiveHigh_ = config.backlightActiveHigh;
  pinMode(backlightPin_, OUTPUT); digitalWrite(backlightPin_, backlightActiveHigh_ ? LOW : HIGH);
  if (config.phyLdoChannel >= 0) {
    esp_ldo_channel_config_t supply{}; supply.chan_id = config.phyLdoChannel; supply.voltage_mv = 2500;
    if (esp_ldo_acquire_channel(&supply, &ldo_) != ESP_OK) { cleanup(); return false; }
  }
  if (config.resetPin >= 0) {
    pinMode(config.resetPin, OUTPUT); digitalWrite(config.resetPin, config.resetActiveLow ? LOW : HIGH);
    delay(config.resetPulseMs);
    digitalWrite(config.resetPin, config.resetActiveLow ? HIGH : LOW); delay(config.resetReleaseMs);
  }
  if (esp_lcd_new_dsi_bus(&config.bus, &bus_) != ESP_OK) { cleanup(); return false; }
  esp_lcd_dbi_io_config_t commandIo{}; commandIo.virtual_channel = config.dpi.virtual_channel;
  commandIo.lcd_cmd_bits = 8; commandIo.lcd_param_bits = 8;
  if (esp_lcd_new_panel_io_dbi(bus_, &commandIo, &io_) != ESP_OK) { cleanup(); return false; }
  for (size_t i = 0; i < config.commandCount; ++i) {
    const auto& command = config.commands[i];
    if ((command.size && !command.data) || esp_lcd_panel_io_tx_param(io_, command.command, command.data, command.size) != ESP_OK) {
      cleanup(); return false;
    }
    if (command.waitMs) delay(command.waitMs); // Finite, reviewed manufacturer startup sequence only.
  }
  auto dpi = config.dpi;
  dpi.num_fbs = 2; dpi.pixel_format = LCD_COLOR_PIXEL_FORMAT_RGB565;
  dpi.in_color_format = LCD_COLOR_FMT_RGB565; dpi.out_color_format = LCD_COLOR_FMT_RGB565;
  dpi.flags.use_dma2d = false; // Direct framebuffer swap, no user-buffer copy.
  if (esp_lcd_new_panel_dpi(bus_, &dpi, &panel_) != ESP_OK) { cleanup(); return false; }
  void* first = nullptr; void* second = nullptr;
  if (esp_lcd_dpi_panel_get_frame_buffer(panel_, 2, &first, &second) != ESP_OK || !first || !second) {
    cleanup(); return false;
  }
  buffers_[0] = static_cast<uint16_t*>(first); buffers_[1] = static_cast<uint16_t*>(second);
  std::memset(first, 0, 1024 * 600 * sizeof(uint16_t)); std::memset(second, 0, 1024 * 600 * sizeof(uint16_t));
  esp_lcd_dpi_panel_event_callbacks_t callbacks{}; callbacks.on_refresh_done = refreshed;
  if (esp_lcd_dpi_panel_register_event_callbacks(panel_, &callbacks, this) != ESP_OK || esp_lcd_panel_init(panel_) != ESP_OK) {
    cleanup(); return false;
  }
  if (!ledcAttach(backlightPin_, config.backlightPwmHz, 8)) { cleanup(); return false; }
  pwm_ = true; ready_ = true; targetBrightness_ = config.brightness; setBrightness(0);
  Serial.println("Panel initialized: 1024x600 RGB565, two buffers; actual FPS not yet measured");
  return true;
}
uint16_t* PanelDriver::drawBuffer() {
  if (!ready_ || !fence_.available(refreshes_.load())) return nullptr;
  if (presented_ && !lightStarted_) { setBrightness(targetBrightness_); lightStarted_ = true; }
  return buffers_[fence_.drawIndex()];
}
bool PanelDriver::present(const uint16_t* pixels, size_t count) {
  if (!ready_ || count != 1024 * 600 || pixels != drawBuffer()) return false;
  if (esp_lcd_panel_draw_bitmap(panel_, 0, 0, 1024, 600, pixels) != ESP_OK) return false;
  presented_ = fence_.presented(refreshes_.load());
  return presented_;
}
