#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

namespace wired_tire {
constexpr size_t kRows = 24, kCols = 32, kPixels = kRows * kCols;
constexpr size_t kValidityBytes = kPixels / 8, kZones = 8;
enum class WheelId : uint8_t { FL = 1, FR = 2, RL = 3, RR = 4 };
enum class FrameState : uint8_t { NotReady = 0, Valid = 1, Partial = 2, Stale = 3, CommError = 4 };
inline bool validWheel(WheelId wheel) {
  const auto value = static_cast<uint8_t>(wheel);
  return value >= 1 && value <= 4;
}
struct ThermalFrame {
  std::array<int16_t, kPixels> temp10{};
  std::array<uint8_t, kValidityBytes> valid{};
  bool isValid(size_t index) const {
    return index < kPixels && (valid[index / 8] & (uint8_t{1} << (index % 8))) != 0;
  }
  void setValid(size_t index, bool value) {
    if (index >= kPixels) return;
    const auto mask = static_cast<uint8_t>(uint8_t{1} << (index % 8));
    if (value) valid[index / 8] |= mask;
    else valid[index / 8] &= static_cast<uint8_t>(~mask);
  }
};
struct FrameMeta {
  WheelId wheel = static_cast<WheelId>(0);
  uint32_t bootId = 0, rxSequence = 0;
  uint64_t receivedUs = 0;
  FrameState state = FrameState::NotReady;
  bool bleEnabled = false, bleConnected = false;
};
struct ZoneResult {
  std::array<int16_t, kZones> temp10{};
  uint8_t validMask = 0, roiRow = 3;
  bool mirrored = false;
  uint32_t configVersion = 1;
};
struct LinkStats {
  uint32_t crcErrors = 0, lengthErrors = 0, timeouts = 0, recoveries = 0, dropped = 0;
};
}
