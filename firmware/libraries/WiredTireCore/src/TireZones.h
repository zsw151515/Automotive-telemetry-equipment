#pragma once
#include "ThermalFrame.h"
namespace wired_tire {
struct ZoneConfig {
  bool mirror = false;
  uint32_t version = 1;
  float maxRateCPerSecond = 160.0f, deadbandC = 0.06f, spikeGateC = 12.0f;
};
struct ZoneFilterState {
  std::array<float, kZones> filtered{}, lastRaw{};
  std::array<uint64_t, kZones> lastZoneUs{};
  uint64_t lastFrameUs = 0;
  float roi = 3.0f;
  uint8_t initializedMask = 0;
  uint32_t configVersion = 0;
  bool hasFrame = false, mirror = false;
};
ZoneResult updateZones(const ThermalFrame& frame, uint64_t rxUs,
                       const ZoneConfig& config, ZoneFilterState& state);
}
