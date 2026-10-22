#include "SnapshotSlave.h"
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <esp_memory_utils.h>
#include <esp_timer.h>
#include <limits>
#include <cstring>
using namespace wired_tire;
void SnapshotSlave::prepared(spi_slave_transaction_t* transaction) {
  auto* self = static_cast<SnapshotSlave*>(transaction->user);
  if (self) gpio_set_level(static_cast<gpio_num_t>(self->ready_), 1);
}
void SnapshotSlave::completed(spi_slave_transaction_t* transaction) {
  auto* self = static_cast<SnapshotSlave*>(transaction->user);
  if (self) gpio_set_level(static_cast<gpio_num_t>(self->ready_), 0);
}
bool SnapshotSlave::queue() {
  if (queued_) return true;
  const auto* tx = tx_.acquire();
  if (!tx) return false;
  if (!esp_ptr_dma_capable(tx)) { tx_.release(); return false; }
  std::memset(rx_, 0, kSnapshotBytes);
  transaction_ = {};
  transaction_.length = kSnapshotBytes * 8;
  transaction_.tx_buffer = tx; transaction_.rx_buffer = rx_; transaction_.user = this;
  if (spi_slave_queue_trans(SPI2_HOST, &transaction_, 0) != ESP_OK) {
    tx_.release(); ++dropped_; return false;
  }
  queued_ = true;
  return true;
}
bool SnapshotSlave::begin(ProbeLink& probe, RaceChronoBle& ble, int sck, int mosi, int miso, int select, int ready) {
  if (enabled_ || sck < 0 || mosi < 0 || miso < 0 || select < 0 || !GPIO_IS_VALID_OUTPUT_GPIO(ready)) return false;
  probe_ = &probe; ble_ = &ble; ready_ = ready;
  pinMode(ready, OUTPUT); digitalWrite(ready, LOW);
  rx_ = static_cast<uint8_t*>(heap_caps_aligned_alloc(16, kSnapshotBytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
  if (!rx_) return false;
  spi_bus_config_t bus{};
  bus.mosi_io_num = mosi; bus.miso_io_num = miso; bus.sclk_io_num = sck;
  bus.quadwp_io_num = bus.quadhd_io_num = -1;
  bus.data4_io_num = bus.data5_io_num = bus.data6_io_num = bus.data7_io_num = -1;
  bus.max_transfer_sz = kSnapshotBytes;
  spi_slave_interface_config_t slave{};
  slave.spics_io_num = select; slave.queue_size = 1; slave.mode = 0;
  slave.post_setup_cb = prepared; slave.post_trans_cb = completed;
  if (spi_slave_initialize(SPI2_HOST, &bus, &slave, SPI_DMA_CH_AUTO) != ESP_OK) {
    heap_caps_free(rx_); rx_ = nullptr; return false;
  }
  enabled_ = true;
  if (!queue()) {
    enabled_ = false; spi_slave_free(SPI2_HOST); heap_caps_free(rx_); rx_ = nullptr; return false;
  }
  Serial.println("SPI slave initialized: mode 0, fixed 1728-byte transactions");
  return true;
}
void SnapshotSlave::poll() {
  if (!enabled_) return;
  if (!queued_) { queue(); return; }
  spi_slave_transaction_t* done = nullptr;
  if (spi_slave_get_trans_result(SPI2_HOST, &done, 0) != ESP_OK) return;
  queued_ = false; tx_.release();
  uint32_t token = 0;
  const auto wheel = probe_->meta().wheel;
  if (done != &transaction_ || done->trans_len != kSnapshotBytes * 8 ||
      !decodeSnapshotRequest(rx_, kSnapshotBytes, wheel, token)) {
    ++dropped_; queue(); return;
  }
  FrameMeta meta = probe_->meta();
  const bool haveFrame = probe_->copyLatest(frame_, meta);
  meta.bleEnabled = ble_->enabled(); meta.bleConnected = ble_->connected();
  auto stats = probe_->stats(); stats.dropped += dropped_;
  const auto now = static_cast<uint64_t>(esp_timer_get_time());
  const uint64_t ageUs = now >= meta.receivedUs ? now - meta.receivedUs : UINT64_MAX;
  const uint64_t ageMs = ageUs / 1000 + (ageUs % 1000 != 0);
  const uint32_t age = haveFrame && ageMs <= UINT32_MAX ? static_cast<uint32_t>(ageMs) : UINT32_MAX;
  if (tx_.prepare(frame_, meta, probe_->zones(), stats, age, token) != EncodeResult::Ok) ++dropped_;
  queue();
}
