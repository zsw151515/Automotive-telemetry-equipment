// 独立有线胎温通道：ESP32-C3 + RS485 MLX90640；不是无线 v3 的替换固件。
#include <Arduino.h>
#include <esp_timer.h>
#include "ChannelConfig.h"
#include "ProbeLink.h"
#if !CONFIG_IDF_TARGET_ESP32C3
#error "wired_tire_channel targets ESP32-C3"
#endif
ProbeLink probe;
uint64_t lastStatusUs = 0;
void setup() {
  Serial.begin(115200);
  Serial.println("Boot wired tire channel 0.1.0 - OFFLINE VALIDATED ONLY");
  Serial.printf("Board=custom C3 channel; wheel=%u RX=%d TX=%d DE=%d\n",
      static_cast<unsigned>(channel_config::wheel), channel_config::rx, channel_config::tx, channel_config::direction);
  Serial.println("Supply=external protected automotive source; voltage not measured");
  if (!channel_config::hardwareEnabled) {
    Serial.println("OFFLINE BUILD: sensor IO and BLE disabled; no approved pinmap");
    return;
  }
  if (!probe.begin(channel_config::wheel, channel_config::rx, channel_config::tx, channel_config::direction))
    Serial.println("Probe interface init FAILED; firmware remains responsive");
}
void loop() {
  const auto now = static_cast<uint64_t>(esp_timer_get_time());
  if (channel_config::hardwareEnabled) probe.poll(now);
  if (now - lastStatusUs >= 1000000) {
    lastStatusUs = now;
    const auto meta = probe.meta(); const auto& stats = probe.stats();
    Serial.printf("Probe status=%u rx=%lu crc=%lu length=%lu timeout=%lu recover=%lu\n",
        static_cast<unsigned>(meta.state), static_cast<unsigned long>(meta.rxSequence),
        static_cast<unsigned long>(stats.crcErrors), static_cast<unsigned long>(stats.lengthErrors),
        static_cast<unsigned long>(stats.timeouts), static_cast<unsigned long>(stats.recoveries));
  }
  yield();
}
