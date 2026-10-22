#include "SnapshotWire.h"
#include <cstring>
namespace wired_tire {
namespace {
void put(uint8_t* out, size_t offset, uint64_t value, size_t bytes) {
  for (size_t i = 0; i < bytes; ++i) out[offset + i] = static_cast<uint8_t>(value >> (8 * i));
}
uint64_t get(const uint8_t* in, size_t offset, size_t bytes) {
  uint64_t value = 0;
  for (size_t i = 0; i < bytes; ++i) value |= uint64_t{in[offset + i]} << (8 * i);
  return value;
}
int16_t signed16(uint64_t bits) {
  return static_cast<int16_t>(bits >= 0x8000 ? static_cast<int32_t>(bits) - 65536 : bits);
}
bool good(const ThermalFrame& frame, const FrameMeta& meta, const ZoneResult& zones) {
  if (!validWheel(meta.wheel) || !meta.bootId || static_cast<unsigned>(meta.state) > 4 ||
      zones.roiRow < 3 || zones.roiRow > 16 || (meta.bleConnected && !meta.bleEnabled)) return false;
  for (size_t i = 0; i < kZones; ++i)
    if ((zones.validMask & (1U << i)) && (zones.temp10[i] < -200 || zones.temp10[i] > 2000)) return false;
  for (size_t i = 0; i < kPixels; ++i)
    if (frame.isValid(i) && (frame.temp10[i] < -400 || frame.temp10[i] > 3000)) return false;
  return true;
}
}
uint32_t wireCrc32(const uint8_t* bytes, size_t size) {
  if (!bytes && size) return 0;
  uint32_t value = 0xFFFFFFFF;
  for (size_t i = 0; i < size; ++i) {
    value ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      value = (value >> 1) ^ ((value & 1) ? 0xEDB88320U : 0);
  }
  return ~value;
}
EncodeResult encodeSnapshot(const ThermalFrame& frame, const FrameMeta& meta, const ZoneResult& zones,
                            const LinkStats& stats, uint32_t ageMs, uint32_t requestToken,
                            uint8_t* out, size_t capacity) {
  if (!out || capacity < kSnapshotBytes) return EncodeResult::InvalidArgument;
  if (!good(frame, meta, zones)) return EncodeResult::InvalidFields;
  std::memset(out, 0, kSnapshotBytes);
  std::memcpy(out, "WTH1", 4);
  out[4] = 1; out[5] = static_cast<uint8_t>(meta.wheel);
  out[6] = static_cast<uint8_t>(meta.state); out[7] = zones.validMask;
  put(out, 8, meta.bootId, 4); put(out, 12, meta.rxSequence, 4); put(out, 16, meta.receivedUs, 8);
  put(out, 24, ageMs, 4); put(out, 28, 1648, 2); out[30] = zones.roiRow;
  out[31] = static_cast<uint8_t>((zones.mirrored ? 1 : 0) | (meta.bleConnected ? 2 : 0) | (meta.bleEnabled ? 4 : 0));
  put(out, 32, zones.configVersion, 4);
  put(out, 36, stats.crcErrors, 4); put(out, 40, stats.lengthErrors, 4); put(out, 44, stats.timeouts, 4);
  put(out, 48, stats.recoveries, 4); put(out, 52, stats.dropped, 4); put(out, 56, requestToken, 4);
  for (size_t i = 0; i < kPixels; ++i) put(out, 64 + i * 2, static_cast<uint16_t>(frame.temp10[i]), 2);
  std::memcpy(out + 1600, frame.valid.data(), kValidityBytes);
  for (size_t i = 0; i < kZones; ++i) put(out, 1696 + i * 2, static_cast<uint16_t>(zones.temp10[i]), 2);
  put(out, kSnapshotCrcOffset, wireCrc32(out, kSnapshotCrcOffset), 4);
  return EncodeResult::Ok;
}
DecodeResult decodeSnapshot(const uint8_t* in, size_t size, WheelId expected, uint32_t expectedToken, Snapshot& out) {
  if (!in || size != kSnapshotBytes) return DecodeResult::LengthMismatch;
  if (std::memcmp(in, "WTH1", 4) != 0) return DecodeResult::InvalidFields;
  if (in[4] != 1) return DecodeResult::UnsupportedVersion;
  if (!validWheel(expected) || in[5] != static_cast<uint8_t>(expected)) return DecodeResult::WrongWheel;
  if (get(in, 56, 4) != expectedToken) return DecodeResult::WrongToken;
  if (get(in, kSnapshotCrcOffset, 4) != wireCrc32(in, kSnapshotCrcOffset)) return DecodeResult::CrcMismatch;
  if (get(in, 28, 2) != 1648 || (in[31] & ~7U)) return DecodeResult::InvalidFields;
  for (size_t i = 60; i < 64; ++i) if (in[i]) return DecodeResult::InvalidFields;
  for (size_t i = 1716; i < kSnapshotBytes; ++i) if (in[i]) return DecodeResult::InvalidFields;
  Snapshot decoded;
  decoded.meta.wheel = expected;
  decoded.meta.state = static_cast<FrameState>(in[6]);
  decoded.meta.bootId = static_cast<uint32_t>(get(in, 8, 4));
  decoded.meta.rxSequence = static_cast<uint32_t>(get(in, 12, 4));
  decoded.meta.receivedUs = get(in, 16, 8);
  decoded.meta.bleConnected = (in[31] & 2) != 0; decoded.meta.bleEnabled = (in[31] & 4) != 0;
  decoded.ageMs = static_cast<uint32_t>(get(in, 24, 4)); decoded.requestToken = expectedToken;
  decoded.zones.validMask = in[7]; decoded.zones.roiRow = in[30]; decoded.zones.mirrored = (in[31] & 1) != 0;
  decoded.zones.configVersion = static_cast<uint32_t>(get(in, 32, 4));
  decoded.stats.crcErrors = static_cast<uint32_t>(get(in, 36, 4));
  decoded.stats.lengthErrors = static_cast<uint32_t>(get(in, 40, 4));
  decoded.stats.timeouts = static_cast<uint32_t>(get(in, 44, 4));
  decoded.stats.recoveries = static_cast<uint32_t>(get(in, 48, 4));
  decoded.stats.dropped = static_cast<uint32_t>(get(in, 52, 4));
  for (size_t i = 0; i < kPixels; ++i) decoded.frame.temp10[i] = signed16(get(in, 64 + i * 2, 2));
  std::memcpy(decoded.frame.valid.data(), in + 1600, kValidityBytes);
  for (size_t i = 0; i < kZones; ++i) decoded.zones.temp10[i] = signed16(get(in, 1696 + i * 2, 2));
  if (!good(decoded.frame, decoded.meta, decoded.zones)) return DecodeResult::InvalidFields;
  out = decoded;
  return DecodeResult::Ok;
}
}
