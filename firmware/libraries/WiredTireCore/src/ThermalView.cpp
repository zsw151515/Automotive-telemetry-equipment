#include "ThermalView.h"
#include "RasterPrimitives.h"
#include <limits>
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace wired_tire {
using raster::fill;using raster::text;using raster::validSurface;
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
namespace {
constexpr uint16_t kInvalidColor = 0x4208;
uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
void formatTemperature(int16_t value, char* buffer, size_t size) {
  const int number = value;
  const int magnitude = std::abs(number);
  std::snprintf(buffer, size, "%s%d.%d", number < 0 ? "-" : "", magnitude / 10, magnitude % 10);
}
uint16_t grey(uint16_t pixel) {
  const unsigned r = ((pixel >> 11) & 31) * 255 / 31, g = ((pixel >> 5) & 63) * 255 / 63, b = (pixel & 31) * 255 / 31;
  const unsigned shade = (r * 3 + g * 6 + b) / 20; // grey and dimmed, explicitly stale
  return rgb(shade, shade, shade);
}
}
uint16_t heatColor(int16_t temperature, bool valid, const ColorScale& scale) {
  if (!valid || temperature < -400 || temperature > 3000 || scale.maximum10 <= scale.minimum10) return kInvalidColor;
  const int position = std::clamp((int{temperature} - scale.minimum10) * 255 /
      (int{scale.maximum10} - scale.minimum10), 0, 255);
  constexpr unsigned anchors[5][3]{{0,32,255},{0,224,255},{32,255,64},{255,224,0},{255,16,0}};
  const unsigned segment = static_cast<unsigned>(std::min(position / 64, 3));
  const unsigned base = segment * 64;
  const unsigned span = segment == 3 ? 63 : 64;
  const unsigned distance = static_cast<unsigned>(position) - base;
  unsigned channels[3]{};
  for (unsigned c = 0; c < 3; ++c)
    channels[c] = (anchors[segment][c] * (span - distance) + anchors[segment + 1][c] * distance) / span;
  return rgb(channels[0], channels[1], channels[2]);
}
bool maximumTemperature(const ThermalFrame& frame, int16_t& output) {
  bool found = false; int16_t maximum = INT16_MIN;
  for (size_t i = 0; i < kPixels; ++i) {
    if (!frame.isValid(i) || frame.temp10[i] < -400 || frame.temp10[i] > 3000) continue;
    if (!found || frame.temp10[i] > maximum) maximum = frame.temp10[i];
    found = true;
  }
  if (found) output = maximum;
  return found;
}
static void renderMap(const ThermalFrame& frame, const ColorScale& scale, PixelSurface& surface, bool dim, bool mirror = false) {
  if (!validSurface(surface)) return;
  const auto ceilDivide = [](size_t value, size_t by) { return value / by + (value % by != 0); };
  for (size_t row = 0; row < kRows; ++row) {
    const size_t firstY = ceilDivide(row * surface.height, kRows), lastY = ceilDivide((row + 1) * surface.height, kRows);
    for (size_t col = 0; col < kCols; ++col) {
      const size_t firstX = ceilDivide(col * surface.width, kCols), lastX = ceilDivide((col + 1) * surface.width, kCols);
      const size_t index = row * kCols + (mirror ? kCols - 1 - col : col);
      auto color = heatColor(frame.temp10[index], frame.isValid(index), scale);
      if (dim) color = grey(color);
      for (size_t y = firstY; y < lastY; ++y)
        std::fill_n(surface.pixels + y * surface.stride + firstX, lastX - firstX, color);
    }
  }
}
void renderHeatmap(const ThermalFrame& frame, const ColorScale& scale, PixelSurface& surface) {
  renderMap(frame, scale, surface, false);
}
void renderHeatmap(const ThermalFrame&frame,const ColorScale&scale,PixelSurface&surface,bool dim,bool mirror){renderMap(frame,scale,surface,dim,mirror);}
void renderDashboard(const std::array<SnapshotDelivery,4>& values, uint64_t nowUs,
                     const ColorScale& scale, PixelSurface& surface, bool simulation) {
  if (!validSurface(surface) || surface.width != 1024 || surface.height != 600) return;
  const uint16_t background = rgb(8,13,20), white = rgb(230,238,245), muted = rgb(140,160,180);
  fill(surface, 0, 0, 1024, 600, background);
  text(surface, 12, 5, scale.maximum10 > scale.minimum10 ? "WIRED TIRE THERMAL" : "SCALE ERROR", 2, white);
  if (simulation) text(surface, 820, 5, "SIMULATED DATA", 2, rgb(255,190,40));
  const char* wheelNames[]{"FL", "FR", "RL", "RR"};
  for (size_t wheel = 0; wheel < 4; ++wheel) {
    const size_t left = (wheel % 2) * 512, top = 24 + (wheel / 2) * 280;
    const auto& sample = values[wheel]; const auto view = updateView(sample, nowUs);
    text(surface, left + 12, top + 5, wheelNames[wheel], 3, white);
    const size_t offset = (top + 32) * surface.stride + left + 12;
    PixelSurface map{surface.pixels + offset,320,240,surface.stride,surface.capacity - offset};
    renderMap(sample.snapshot.frame, scale, map, !view.fresh, sample.snapshot.zones.mirrored);
    const auto& meta = sample.snapshot.meta;
    const char* status = !meta.bootId || meta.state == FrameState::NotReady ? "NO DATA" :
        meta.state == FrameState::CommError ? "FAIL" : !view.fresh ? "STALE" :
        meta.state == FrameState::Partial ? "PART" : "LIVE";
    text(surface, left + 350, top + 32, status, 2, view.fresh ? rgb(80,230,150) : rgb(255,190,40));
    text(surface, left + 350, top + 68, "MAX C", 2, muted);
    int16_t maximum = 0; char label[24];
    if (maximumTemperature(sample.snapshot.frame, maximum)) formatTemperature(maximum, label, sizeof(label));
    else std::snprintf(label, sizeof(label), "--.-");
    text(surface, left + 350, top + 92, label, 4, view.fresh ? white : muted);
    const char* ble = !meta.bootId ? "BLE --" : !meta.bleEnabled ? "BLE OFF" : meta.bleConnected ? "BLE ON" : "BLE WAIT";
    text(surface, left + 350, top + 150, ble, 2, muted);
    text(surface, left + 350, top + 185, "AGE", 2, muted);
    if (!meta.bootId) std::snprintf(label, sizeof(label), "--");
    else std::snprintf(label, sizeof(label), "%luMS", static_cast<unsigned long>(view.ageMs));
    text(surface, left + 350, top + 207, label, 2, muted);
  }
  fill(surface, 511, 24, 2, 560, rgb(45,60,75)); fill(surface, 0, 303, 1024, 2, rgb(45,60,75));
  char low[24], high[24]; formatTemperature(scale.minimum10, low, sizeof(low)); formatTemperature(scale.maximum10, high, sizeof(high));
  text(surface, 12, 588, low, 1, white); text(surface, 344, 588, high, 1, white);
  text(surface, 410, 588, "C - SHARED SCALE", 1, muted);
  for (size_t x = 0; x < 256; ++x) {
    const auto temperature = static_cast<int16_t>(scale.minimum10 + (int{scale.maximum10} - scale.minimum10) * static_cast<int>(x) / 255);
    fill(surface, 80 + x, 589, 1, 5, heatColor(temperature, true, scale));
  }
}
}
