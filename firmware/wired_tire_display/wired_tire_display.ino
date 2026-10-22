// 新中控显示主控：ESP32-P4。默认离线构建不操作未核准的硬件。
#include <Arduino.h>
#include <esp_timer.h>
#include "DisplayConfig.h"
#include "SnapshotMaster.h"
#if !CONFIG_IDF_TARGET_ESP32P4
#error "wired_tire_display targets ESP32-P4"
#endif
SnapshotMaster snapshotMaster;
std::array<wired_tire::SnapshotDelivery, 4> deliveries;
wired_tire::SnapshotDelivery incoming;
uint64_t lastDisplayStatusUs = 0;
void setup() {
  Serial.begin(115200);
  Serial.println("Boot wired tire display 0.1.0 - OFFLINE VALIDATED ONLY");
  Serial.println("Custom PCB, 7-inch target 1024x600; external protected supply, voltage not measured");
  if (!display_config::hardwareEnabled) {
    Serial.println("OFFLINE BUILD: SPI and panel IO disabled; no approved profile");
    return;
  }
  if (!snapshotMaster.begin(display_config::sck, display_config::mosi, display_config::miso,
      display_config::selects, display_config::ready))
    Serial.println("SPI master init FAILED; main loop remains responsive");
}
void loop() {
  const auto now = static_cast<uint64_t>(esp_timer_get_time());
  if (display_config::hardwareEnabled) {
    for (unsigned i = 0; i < 4; ++i) {
      if (snapshotMaster.pollWheel(static_cast<wired_tire::WheelId>(i + 1), incoming)) deliveries[i] = incoming;
    }
  }
  if (now - lastDisplayStatusUs >= 1000000) {
    lastDisplayStatusUs = now;
    for (unsigned i = 0; i < 4; ++i) {
      const auto view = wired_tire::updateView(deliveries[i], static_cast<uint64_t>(esp_timer_get_time()));
      Serial.printf("Wheel %u fresh=%u age_ms=%lu BLE=%u\n", i + 1, view.fresh,
          static_cast<unsigned long>(view.ageMs), deliveries[i].snapshot.meta.bleConnected);
    }
  }
  yield();
}
