#pragma once
#include <Arduino.h>
#include <driver/spi_master.h>
#include <WiredTireCore.h>
class SnapshotMaster {
 public:
  bool begin(int sck, int mosi, int miso, const std::array<int,4>& selects, const std::array<int,4>& ready);
  bool pollWheel(wired_tire::WheelId, wired_tire::SnapshotDelivery&);
 private:
  struct Channel {
    spi_device_handle_t device = nullptr;
    spi_transaction_t transaction{};
    uint8_t* tx = nullptr;
    uint8_t* rx = nullptr;
    wired_tire::ProbeSchedule schedule{100000, 250000, 500000};
    uint64_t startedUs = 0;
    uint32_t token = 0;
    int ready = -1;
  };
  std::array<Channel,4> channels_;
  uint32_t nextToken_ = 0;
  int active_ = -1;
  bool enabled_ = false;
  void cleanup();
};
