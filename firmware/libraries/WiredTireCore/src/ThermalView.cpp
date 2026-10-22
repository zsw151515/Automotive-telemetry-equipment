#include "ThermalView.h"
#include <limits>
namespace wired_tire {
ViewState updateView(const SnapshotDelivery& delivery, uint64_t nowLocalUs) {
  ViewState result;
  if (nowLocalUs < delivery.receivedLocalUs) return result;
  const uint64_t elapsedUs = nowLocalUs - delivery.receivedLocalUs;
  const uint64_t elapsedMs = elapsedUs / 1000 + (elapsedUs % 1000 != 0);
  const uint64_t totalMs = uint64_t{delivery.snapshot.ageMs} + delivery.requestElapsedMs + elapsedMs;
  result.ageMs = static_cast<uint32_t>(totalMs > std::numeric_limits<uint32_t>::max()
      ? std::numeric_limits<uint32_t>::max() : totalMs);
  const auto& meta = delivery.snapshot.meta;
  result.fresh = validWheel(meta.wheel) && meta.bootId &&
      (meta.state == FrameState::Valid || meta.state == FrameState::Partial) && result.ageMs <= 1500;
  return result;
}
}
