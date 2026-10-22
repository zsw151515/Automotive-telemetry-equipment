#pragma once
#include <cstdint>
namespace wired_tire {
bool isFresh(uint64_t nowUs, uint64_t lastGoodUs);
enum class ProbeAction : uint8_t { None, Request, Timeout };
class ProbeSchedule {
 public:
  explicit ProbeSchedule(uint64_t timeoutUs = 400000, uint64_t intervalUs = 250000, uint64_t retryUs = 500000)
      : timeoutUs_(timeoutUs ? timeoutUs : 400000), intervalUs_(intervalUs ? intervalUs : 250000), retryUs_(retryUs ? retryUs : 500000) {}
  ProbeAction poll(uint64_t nowUs);
  void response(uint64_t nowUs, bool accepted);
  bool hasGoodResponse() const { return good_; }
  uint64_t lastGoodUs() const { return lastGood_; }
  bool pending() const { return pending_; }
 private:
  uint64_t timeoutUs_, intervalUs_, retryUs_;
  uint64_t lastClock_ = 0, requested_ = 0, nextRequest_ = 0, lastGood_ = 0;
  bool clockSet_ = false, pending_ = false, good_ = false;
};
}
