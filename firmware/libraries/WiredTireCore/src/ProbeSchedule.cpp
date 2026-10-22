#include "ProbeSchedule.h"
#include <limits>
namespace wired_tire {
namespace {
uint64_t later(uint64_t now, uint64_t delay) {
  return now > std::numeric_limits<uint64_t>::max() - delay
      ? std::numeric_limits<uint64_t>::max() : now + delay;
}
}
bool isFresh(uint64_t nowUs, uint64_t lastGoodUs) {
  return nowUs >= lastGoodUs && nowUs - lastGoodUs <= 1500000;
}
ProbeAction ProbeSchedule::poll(uint64_t nowUs) {
  if (clockSet_ && nowUs < lastClock_) {
    pending_ = false; good_ = false; nextRequest_ = nowUs;
  }
  clockSet_ = true; lastClock_ = nowUs;
  if (pending_) {
    if (nowUs - requested_ >= timeoutUs_) {
      pending_ = false; nextRequest_ = later(nowUs, retryUs_); return ProbeAction::Timeout;
    }
    return ProbeAction::None;
  }
  if (nowUs >= nextRequest_) {
    pending_ = true; requested_ = nowUs; nextRequest_ = later(nowUs, intervalUs_);
    return ProbeAction::Request;
  }
  return ProbeAction::None;
}
void ProbeSchedule::response(uint64_t nowUs, bool accepted) {
  if (!pending_) return;
  pending_ = false;
  if (nowUs < lastClock_) { good_ = false; clockSet_ = false; nextRequest_ = nowUs; return; }
  lastClock_ = nowUs;
  if (accepted) { lastGood_ = nowUs; good_ = true; }
  else nextRequest_ = later(nowUs, retryUs_);
}
}
