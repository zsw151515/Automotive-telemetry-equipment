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
enum class EncodeResult : uint8_t { Ok, InvalidArgument, InvalidFields, Busy };
enum class DecodeResult : uint8_t {
  Ok, LengthMismatch, InvalidFields, UnsupportedVersion, WrongWheel, WrongToken, CrcMismatch
};
uint32_t wireCrc32(const uint8_t* bytes, size_t size);
EncodeResult encodeSnapshot(const ThermalFrame&, const FrameMeta&, const ZoneResult&, const LinkStats&,
                            uint32_t ageMs, uint32_t requestToken, uint8_t*, size_t);
DecodeResult decodeSnapshot(const uint8_t*, size_t, WheelId expected, uint32_t expectedToken, Snapshot&);
bool encodeSnapshotRequest(WheelId, uint32_t token, uint8_t*, size_t);
bool decodeSnapshotRequest(const uint8_t*, size_t, WheelId expected, uint32_t& token);
// Single-owner main-task buffer. Do not call concurrently from ISR/worker tasks.
class SnapshotTxBuffer {
 public:
  EncodeResult prepare(const ThermalFrame&, const FrameMeta&, const ZoneResult&, const LinkStats&, uint32_t ageMs, uint32_t token);
  const uint8_t* acquire();
  void release() { leased_ = false; }
 private:
  alignas(16) std::array<uint8_t, kSnapshotBytes> storage_{};
  bool leased_ = false;
};
}
