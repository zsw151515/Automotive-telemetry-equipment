#pragma once
#include <Arduino.h>
#include "BrakeChannelConfig.h"
class DualBrakeProbe {
 public:
  WT_OFFLINE_LINK bool begin(const BrakeHardwareConfig&);
  WT_OFFLINE_LINK void poll(uint64_t now);
  const std::array<wired_brake::BrakeReading,2>& readings()const{return readings_;}
  const wired_tire::LinkStats& stats()const{return stats_;}
  uint8_t enabledMask()const{return mask_;}
 private:
  HardwareSerial port0_{0},port1_{1};BrakeHardwareConfig cfg_{};
  std::array<wired_brake::BrakePoller,2> pollers_{};std::array<wired_brake::BrakeReading,2>readings_{};
  uint8_t bytes_[2][16]{};size_t count_[2]{};bool receiving_[2]{};
  wired_tire::LinkStats stats_{};bool enabled_=false,clockStarted_=false;uint64_t start_=0;uint8_t mask_=0;
};
