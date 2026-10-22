#pragma once
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#define CHECK(expr) do { if (!(expr)) throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + " " #expr); } while (false)
inline void runTest(const char* name, const std::function<void()>& test) {
  try { test(); std::cout << "PASS " << name << '\n'; }
  catch (const std::exception& error) { std::cerr << "FAIL " << name << ": " << error.what() << '\n'; std::exit(1); }
}
// Synthetic fixture checksum, independent of the production CRC entry point.
inline uint16_t fixtureCrc(const std::vector<uint8_t>& bytes, size_t count) {
  uint16_t result = 0xFFFF;
  for (size_t i = 0; i < count; ++i) {
    result ^= bytes[i];
    for (unsigned bit = 0; bit < 8; ++bit)
      result = static_cast<uint16_t>((result >> 1) ^ ((result & 1) ? 0xA001 : 0));
  }
  return result;
}
inline void repairFixtureCrc(std::vector<uint8_t>& bytes) {
  const auto crc = fixtureCrc(bytes, bytes.size() - 2);
  bytes[bytes.size() - 2] = static_cast<uint8_t>(crc);
  bytes.back() = static_cast<uint8_t>(crc >> 8);
}
inline std::vector<uint8_t> yfFixture() {
  std::vector<uint8_t> bytes(1576, 0);
  bytes[0] = 1; bytes[1] = 0x65; bytes[2] = 0x06; bytes[3] = 0x22;
  bytes[36] = 0xFF; bytes[37] = 0x9C; // pixel 0: -10.0 C
  bytes[1570] = 0x07; bytes[1571] = 0xD0; // pixel 767: 200.0 C
  bytes[1572] = 0x12; bytes[1573] = 0x34; // version, never a pixel
  repairFixtureCrc(bytes);
  return bytes;
}
