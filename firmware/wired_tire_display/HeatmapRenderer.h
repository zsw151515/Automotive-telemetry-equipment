#pragma once
#include "PanelDriver.h"
#include <TelemetryUi.h>
class HeatmapRenderer {
 public:
  WT_OFFLINE_LINK void poll(uint64_t nowUs, const std::array<wired_tire::SnapshotDelivery,4>&, const wired_tire::ColorScale&, PanelDriver&);
  WT_OFFLINE_LINK void poll(uint64_t,const std::array<wired_tire::SnapshotDelivery,4>&,const wired_tire::ColorScale&,PanelDriver&,wired_brake::TelemetryUi&);
  uint32_t submitted() const { return submitted_; }
 private:
  uint64_t nextDrawUs_ = 0;
  uint32_t submitted_ = 0;
};
