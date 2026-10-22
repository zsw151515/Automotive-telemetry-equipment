#pragma once
#include "ThermalFrame.h"
namespace wired_tire {
enum class ParseResult : uint8_t {
  Ok, TooShort, TooLong, WrongAddress, WrongFunction, Exception,
  LengthMismatch, CrcMismatch, UnsupportedLayout
};
constexpr size_t kYfRegisterBytes = 1570;
constexpr size_t kYfResponseBytes = kYfRegisterBytes + 6;
constexpr size_t kMaxProbeResponse = 2048;
uint16_t modbusCrc(const uint8_t* bytes, size_t size);
ParseResult decodeYf65(const uint8_t* bytes, size_t size, uint8_t expectedAddress, ThermalFrame& output);
}
