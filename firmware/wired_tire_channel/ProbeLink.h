#pragma once
#include <Arduino.h>
#include <WiredTireCore.h>
class ProbeLink {
 public:
  bool begin(wired_tire::WheelId wheel, int rx, int tx, int direction);
  void poll(uint64_t nowUs);
  bool copyLatest(wired_tire::ThermalFrame&, wired_tire::FrameMeta&) const;
  const wired_tire::ZoneResult& zones() const { return zones_; }
  const wired_tire::LinkStats& stats() const { return stats_; }
  wired_tire::FrameMeta meta() const { return meta_; }
 private:
  void finish(uint64_t nowUs);
  wired_tire::ThermalFrame latest_, candidate_;
  wired_tire::FrameMeta meta_;
  wired_tire::ZoneConfig config_;
  wired_tire::ZoneFilterState filter_;
  wired_tire::ZoneResult zones_;
  wired_tire::LinkStats stats_;
  wired_tire::ProbeSchedule schedule_;
  uint8_t response_[wired_tire::kMaxProbeResponse]{};
  size_t count_ = 0;
  bool enabled_ = false, receiving_ = false, haveFrame_ = false;
};
