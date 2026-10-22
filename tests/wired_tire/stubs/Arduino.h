#pragma once
// Only hardware API boundaries are replaced; production driver methods run unchanged.
#include <cstddef>
#include <cstdint>
#include <deque>
#include <vector>
constexpr int SERIAL_8N2 = 0;
struct TestSerial {
  std::deque<uint8_t> rx;
  std::vector<uint8_t> tx;
  void begin(int, int = 0, int = -1, int = -1) {}
  void end() {}
  int available() const { return static_cast<int>(rx.size()); }
  int read() { if (rx.empty()) return -1; const int v = rx.front(); rx.pop_front(); return v; }
  size_t write(const uint8_t* data, size_t size) { tx.insert(tx.end(), data, data + size); return size; }
  void println(const char*) {}
  template<class... Args> void printf(const char*, Args...) {}
  void inject(const std::vector<uint8_t>& bytes) { rx.insert(rx.end(), bytes.begin(), bytes.end()); }
};
inline TestSerial Serial, Serial1;
