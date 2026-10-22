#pragma once
#include "ProbeLink.h"
#include "RaceChronoBle.h"
#include <driver/spi_slave.h>
class SnapshotSlave {
 public:
  bool begin(ProbeLink&, RaceChronoBle&, int sck, int mosi, int miso, int select, int ready);
  void poll();
 private:
  static void prepared(spi_slave_transaction_t*);
  static void completed(spi_slave_transaction_t*);
  bool queue();
  ProbeLink* probe_ = nullptr;
  RaceChronoBle* ble_ = nullptr;
  wired_tire::SnapshotTxBuffer tx_;
  wired_tire::ThermalFrame frame_;
  spi_slave_transaction_t transaction_{};
  uint8_t* rx_ = nullptr;
  int ready_ = -1;
  uint32_t dropped_ = 0;
  bool enabled_ = false, queued_ = false;
};
