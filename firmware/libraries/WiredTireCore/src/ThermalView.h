#pragma once
#include "SnapshotWire.h"
namespace wired_tire {
struct SnapshotDelivery {
  Snapshot snapshot;
  uint64_t receivedLocalUs = 0;
  uint32_t requestElapsedMs = 0;
};
struct ViewState { bool fresh = false; uint32_t ageMs = 0xFFFFFFFF; };
ViewState updateView(const SnapshotDelivery&, uint64_t nowLocalUs);
}
