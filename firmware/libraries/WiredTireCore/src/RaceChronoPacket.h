#pragma once
#include "ThermalFrame.h"
namespace wired_tire {
struct LegacyAuxFields {
  std::array<uint8_t, 3> characteristic1{}, characteristic2{}, characteristic3{};
};
bool buildRaceChronoPacket(uint8_t characteristic, const ZoneResult& zones,
                          const LegacyAuxFields& aux, uint8_t out[20]);
bool canNotify(const FrameMeta& meta, const ZoneResult& zones, uint64_t nowUs, bool connected);
bool rejsaName(WheelId wheel, const uint8_t mac[6], char* out, size_t capacity);
}
