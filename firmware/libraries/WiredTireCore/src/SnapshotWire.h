#pragma once
#include "ThermalFrame.h"
namespace wired_tire {
constexpr size_t kSnapshotBytes = 1728;
constexpr size_t kSnapshotCrcOffset = 1712;
struct Snapshot {
  ThermalFrame frame;
  FrameMeta meta;
  ZoneResult zones;
  LinkStats stats;
  uint32_t ageMs = 0xFFFFFFFF, requestToken = 0;
};
enum class EncodeResult : uint8_t { Ok, InvalidArgument, InvalidFields };
enum class DecodeResult : uint8_t {
  Ok, LengthMismatch, InvalidFields, UnsupportedVersion, WrongWheel, WrongToken, CrcMismatch
};
uint32_t wireCrc32(const uint8_t* bytes, size_t size);
EncodeResult encodeSnapshot(const ThermalFrame&, const FrameMeta&, const ZoneResult&, const LinkStats&,
                            uint32_t ageMs, uint32_t requestToken, uint8_t*, size_t);
DecodeResult decodeSnapshot(const uint8_t*, size_t, WheelId expected, uint32_t expectedToken, Snapshot&);
}
