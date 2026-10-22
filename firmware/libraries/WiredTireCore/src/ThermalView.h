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
struct ColorScale { int16_t minimum10 = 200, maximum10 = 1400; };
struct PixelSurface {
  uint16_t* pixels = nullptr;
  size_t width = 0, height = 0, stride = 0, capacity = 0;
};
// Main-task state only; the panel ISR supplies a monotonic wrapping refresh counter.
class DisplaySwapFence {
 public:
  bool available(uint32_t refresh) {
    if (pending_ && static_cast<uint32_t>(refresh - submitted_) >= 2) pending_ = false;
    return !pending_;
  }
  bool presented(uint32_t refresh) {
    if (pending_) return false;
    submitted_ = refresh; pending_ = true; draw_ = 1 - draw_; return true;
  }
  unsigned drawIndex() const { return draw_; }
 private:
  uint32_t submitted_ = 0;
  unsigned draw_ = 1;
  bool pending_ = false;
};
uint16_t heatColor(int16_t temperature, bool valid, const ColorScale&);
bool maximumTemperature(const ThermalFrame&, int16_t& output);
void renderHeatmap(const ThermalFrame&, const ColorScale&, PixelSurface&);
void renderDashboard(const std::array<SnapshotDelivery,4>&, uint64_t nowUs, const ColorScale&, PixelSurface&, bool simulation = false);
}
