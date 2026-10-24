#pragma once
#include <Arduino.h>
#include <driver/spi_slave.h>
#include "BrakeChannelConfig.h"
class BrakeSpiSlave {
 public:
  WT_OFFLINE_LINK bool begin(const BrakeHardwareConfig&);
  WT_OFFLINE_LINK void poll(uint64_t now);
  bool takeCommand(wired_brake::BrakeCommand&);
  void publish(const wired_brake::BrakeTelemetry&s){state_=s;}
 private:
  static void prepared(spi_slave_transaction_t*);static void completed(spi_slave_transaction_t*);
  bool queue();int ready_=-1;bool enabled_=false,queued_=false,hasCommand_=false;
  wired_brake::BrakeCommand command_{};wired_brake::BrakeTelemetry state_{};
  spi_slave_transaction_t transaction_{};uint8_t*tx_=nullptr,*rx_=nullptr;
};
