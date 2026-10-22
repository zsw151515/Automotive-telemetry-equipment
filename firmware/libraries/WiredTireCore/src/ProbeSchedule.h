#pragma once
#include <cstdint>
namespace wired_tire {
bool isFresh(uint64_t nowUs, uint64_t lastGoodUs);
enum class ProbeAction : uint8_t { None, Request, Timeout };
class ProbeSchedule {
 public:
  ProbeAction poll(uint64_t nowUs);
  void response(uint64_t nowUs, bool accepted);
  bool hasGoodResponse() const { return good_; }
  uint64_t lastGoodUs() const { return lastGood_; }
 private:
  uint64_t lastClock_ = 0, requested_ = 0, nextRequest_ = 0, lastGood_ = 0;
  bool clockSet_ = false, pending_ = false, good_ = false;
};
}
