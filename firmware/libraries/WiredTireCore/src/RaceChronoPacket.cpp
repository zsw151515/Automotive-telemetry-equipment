#include "RaceChronoPacket.h"
#include "ProbeSchedule.h"
#include <cstdio>
#include <cstring>
namespace wired_tire {
namespace {
bool goodZones(const ZoneResult& zones) {
  if (zones.validMask != 0xFF) return false;
  for (auto value : zones.temp10) if (value < -200 || value > 2000) return false;
  return true;
}
}
bool buildRaceChronoPacket(uint8_t characteristic, const ZoneResult& zones,
                          const LegacyAuxFields& aux, uint8_t out[20]) {
  if (!out || characteristic < 1 || characteristic > 3 || !goodZones(zones)) return false;
  uint8_t packet[20]{};
  packet[0] = 2;
  const auto& header = characteristic == 1 ? aux.characteristic1 :
      characteristic == 2 ? aux.characteristic2 : aux.characteristic3;
  for (size_t i = 0; i < 3; ++i) packet[i + 1] = header[i];
  for (size_t zone = 0; zone < kZones; ++zone) {
    const auto bits = static_cast<uint16_t>(zones.temp10[zone]);
    packet[4 + zone * 2] = static_cast<uint8_t>(bits);
    packet[5 + zone * 2] = static_cast<uint8_t>(bits >> 8);
  }
  std::memcpy(out, packet, sizeof(packet));
  return true;
}
bool canNotify(const FrameMeta& meta, const ZoneResult& zones, uint64_t nowUs, bool connected) {
  return connected && validWheel(meta.wheel) &&
      (meta.state == FrameState::Valid || meta.state == FrameState::Partial) &&
      isFresh(nowUs, meta.receivedUs) && goodZones(zones);
}
bool rejsaName(WheelId wheel, const uint8_t mac[6], char* out, size_t capacity) {
  if (!validWheel(wheel) || !mac || !out || capacity < 20) return false;
  const char* wheelNames[]{"FL", "FR", "RL", "RR"};
  std::snprintf(out, capacity, "RejsaRubber%s%02X%02X%02X",
      wheelNames[static_cast<unsigned>(wheel) - 1], mac[3], mac[4], mac[5]);
  return true;
}
}
