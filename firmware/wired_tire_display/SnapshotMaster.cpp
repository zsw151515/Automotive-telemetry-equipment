#include "SnapshotMaster.h"
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <esp_timer.h>
using namespace wired_tire;
void SnapshotMaster::cleanup() {
  for (auto& channel : channels_) {
    if (channel.device) { spi_bus_remove_device(channel.device); channel.device = nullptr; }
    heap_caps_free(channel.tx); heap_caps_free(channel.rx); channel.tx = channel.rx = nullptr;
  }
}
bool SnapshotMaster::begin(int sck, int mosi, int miso, const std::array<int,4>& selects, const std::array<int,4>& ready) {
  if (enabled_) return false;
  const int pins[]{sck, mosi, miso, selects[0], selects[1], selects[2], selects[3], ready[0], ready[1], ready[2], ready[3]};
  for (size_t i = 0; i < 11; ++i) {
    if (!GPIO_IS_VALID_GPIO(pins[i])) return false;
    for (size_t j = 0; j < i; ++j) if (pins[i] == pins[j]) return false;
  }
  spi_bus_config_t bus{};
  bus.mosi_io_num = mosi; bus.miso_io_num = miso; bus.sclk_io_num = sck;
  bus.quadwp_io_num = bus.quadhd_io_num = -1;
  bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
  bus.max_transfer_sz = kSnapshotBytes;
  if (spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO) != ESP_OK) return false;
  for (size_t i = 0; i < 4; ++i) {
    auto& channel = channels_[i];
    channel.ready = ready[i]; pinMode(ready[i], INPUT);
    channel.tx = static_cast<uint8_t*>(heap_caps_aligned_alloc(16, kSnapshotBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    channel.rx = static_cast<uint8_t*>(heap_caps_aligned_alloc(16, kSnapshotBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    spi_device_interface_config_t device{};
    device.clock_speed_hz = 1000000; device.mode = 0; device.spics_io_num = selects[i]; device.queue_size = 1;
    if (!channel.tx || !channel.rx || spi_bus_add_device(SPI2_HOST, &device, &channel.device) != ESP_OK) {
      cleanup(); spi_bus_free(SPI2_HOST); return false;
    }
  }
  nextToken_ = esp_random(); // P4 reboot must not reuse deterministic pending request 1.
  enabled_ = true;
  Serial.println("SPI master initialized: four independent CS, mode 0, 1 MHz");
  return true;
}
bool SnapshotMaster::pollWheel(WheelId wheel, SnapshotDelivery& output) {
  if (!enabled_ || !validWheel(wheel)) return false;
  const int index = static_cast<int>(wheel) - 1;
  auto& channel = channels_[index];
  const auto now = static_cast<uint64_t>(esp_timer_get_time());
  if (active_ == index) {
    spi_transaction_t* done = nullptr;
    if (spi_device_get_trans_result(channel.device, &done, 0) != ESP_OK) return false;
    active_ = -1;
    if (!channel.schedule.pending() || now < channel.startedUs || now - channel.startedUs >= 100000) {
      channel.schedule.poll(now); return false;
    }
    Snapshot decoded;
    if (done != &channel.transaction ||
        decodeSnapshot(channel.rx, kSnapshotBytes, wheel, channel.token, decoded) != DecodeResult::Ok) return false;
    channel.schedule.response(now, true);
    output.snapshot = decoded; output.receivedLocalUs = now;
    const uint64_t elapsed = now - channel.startedUs;
    output.requestElapsedMs = static_cast<uint32_t>(elapsed / 1000 + (elapsed % 1000 != 0));
    return true;
  }
  if (active_ >= 0) return false;
  const auto action = channel.schedule.poll(now);
  if (action == ProbeAction::Timeout) return false;
  if (action == ProbeAction::Request) {
    channel.startedUs = now;
    channel.token = ++nextToken_;
    if (!channel.token) channel.token = ++nextToken_;
    encodeSnapshotRequest(wheel, channel.token, channel.tx, kSnapshotBytes);
  }
  if (!channel.schedule.pending() || digitalRead(channel.ready) != HIGH) return false;
  channel.transaction = {};
  channel.transaction.length = kSnapshotBytes * 8;
  channel.transaction.tx_buffer = channel.tx; channel.transaction.rx_buffer = channel.rx;
  if (spi_device_queue_trans(channel.device, &channel.transaction, 0) == ESP_OK) active_ = index;
  return false;
}
