// 独立有线胎温通道：ESP32-C3 + RS485 MLX90640；不是无线 v3 的替换固件。
#include <Arduino.h>
#include <esp_timer.h>
#include "ChannelConfig.h"
#include "ProbeLink.h"
#include "RaceChronoBle.h"
#include "SnapshotSlave.h"
#include <driver/gpio.h>
#if !CONFIG_IDF_TARGET_ESP32C3
#error "wired_tire_channel targets ESP32-C3"
#endif
ProbeLink probe;
RaceChronoBle raceChrono;
SnapshotSlave snapshotSlave;
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
  const int pins[]{channel_config::rx, channel_config::tx, channel_config::direction, channel_config::sck,
      channel_config::mosi, channel_config::miso, channel_config::select, channel_config::ready};
  for (unsigned i = 0; i < 8; ++i) {
    if (!GPIO_IS_VALID_GPIO(pins[i])) { Serial.println("Invalid pin profile; all sensor IO disabled"); return; }
    for (unsigned j = 0; j < i; ++j)
      if (pins[i] == pins[j]) { Serial.println("Pin conflict; all sensor IO disabled"); return; }
#if !ARDUINO_USB_CDC_ON_BOOT
    if (pins[i] == 20 || pins[i] == 21) { Serial.println("UART0 debug pin conflict"); return; }
#endif
  }
  if (!probe.begin(channel_config::wheel, channel_config::rx, channel_config::tx, channel_config::direction))
    Serial.println("Probe interface init FAILED; firmware remains responsive");
  raceChrono.begin(channel_config::wheel, channel_config::auxiliaryFields, channel_config::bleAuxVerified);
  if (!snapshotSlave.begin(probe, raceChrono, channel_config::sck, channel_config::mosi, channel_config::miso,
      channel_config::select, channel_config::ready)) Serial.println("SPI slave init FAILED");
}
void loop() {
  const auto now = static_cast<uint64_t>(esp_timer_get_time());
  if (channel_config::hardwareEnabled) {
    probe.poll(now);
    raceChrono.poll(now, probe.meta(), probe.zones());
    snapshotSlave.poll();
  }
  if (now - lastStatusUs >= 1000000) {
    lastStatusUs = now;
    const auto meta = probe.meta(); const auto& stats = probe.stats();
    Serial.printf("Probe status=%u rx=%lu crc=%lu length=%lu timeout=%lu recover=%lu\n",
        static_cast<unsigned>(meta.state), static_cast<unsigned long>(meta.rxSequence),
        static_cast<unsigned long>(stats.crcErrors), static_cast<unsigned long>(stats.lengthErrors),
        static_cast<unsigned long>(stats.timeouts), static_cast<unsigned long>(stats.recoveries));
    Serial.printf("RaceChrono BLE enabled=%u connected=%u notify_attempts=%lu\n", raceChrono.enabled(), raceChrono.connected(),
        static_cast<unsigned long>(raceChrono.notificationAttempts()));
  }
  yield();
}
