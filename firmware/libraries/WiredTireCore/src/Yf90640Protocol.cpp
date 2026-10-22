#include "Yf90640Protocol.h"
namespace wired_tire {
size_t expectedYfResponseBytes(const uint8_t* bytes, size_t size) {
  if (!bytes || size < 2) return 0;
  if (bytes[1] == 0xE5) return 5;
  if (bytes[1] != 0x65) return kMaxProbeResponse + 1;
  if (size < 4) return 0;
  return ((size_t{bytes[2]} << 8) | bytes[3]) + 6;
}
uint16_t modbusCrc(const uint8_t* bytes, size_t size) {
  if (!bytes && size) return 0;
  uint16_t crc = 0xFFFF;
  for (size_t i = 0; i < size; ++i) {
    crc ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      crc = static_cast<uint16_t>((crc >> 1) ^ ((crc & 1) ? 0xA001 : 0));
  }
  return crc;
}
ParseResult decodeYf65(const uint8_t* bytes, size_t size, uint8_t expectedAddress, ThermalFrame& output) {
  if (size > kMaxProbeResponse) return ParseResult::TooLong;
  if (!bytes || size < 5) return ParseResult::TooShort;
  if (expectedAddress == 0 || expectedAddress > 247 || bytes[0] != expectedAddress) return ParseResult::WrongAddress;
  if (bytes[1] & 0x80) return ParseResult::Exception;
  if (bytes[1] != 0x65) return ParseResult::WrongFunction;
  if (size < 6) return ParseResult::TooShort;
  const size_t dataBytes = (size_t{bytes[2]} << 8) | bytes[3];
  if (dataBytes + 6 != size) return ParseResult::LengthMismatch;
  const uint16_t expectedCrc = static_cast<uint16_t>(bytes[size - 2] | (uint16_t{bytes[size - 1]} << 8));
  if (modbusCrc(bytes, size - 2) != expectedCrc) return ParseResult::CrcMismatch;
  if (dataBytes != kYfRegisterBytes) return ParseResult::UnsupportedLayout;
  ThermalFrame decoded;
  for (size_t i = 0; i < kPixels; ++i) {
    const size_t offset = 4 + 0x0010 * 2 + i * 2;
    const uint16_t bits = static_cast<uint16_t>((uint16_t{bytes[offset]} << 8) | bytes[offset + 1]);
    const int32_t value = bits >= 0x8000 ? static_cast<int32_t>(bits) - 65536 : bits;
    decoded.temp10[i] = static_cast<int16_t>(value);
    decoded.setValid(i, value >= -400 && value <= 3000);
  }
  output = decoded;
  return ParseResult::Ok;
}
}
