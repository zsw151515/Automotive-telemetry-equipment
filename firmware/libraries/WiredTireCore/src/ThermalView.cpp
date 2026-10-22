#include "ThermalView.h"
#include <limits>
#include <algorithm>
#include <cmath>
#include <cstdio>
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
namespace {
constexpr uint16_t kInvalidColor = 0x4208;
bool validSurface(const PixelSurface& surface) {
  if (!surface.pixels || !surface.width || !surface.height || surface.stride < surface.width ||
      surface.capacity < surface.width || surface.width > SIZE_MAX / kCols || surface.height > SIZE_MAX / kRows) return false;
  return surface.height - 1 <= (surface.capacity - surface.width) / surface.stride;
}
uint16_t rgb(unsigned r, unsigned g, unsigned b) {
  return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
std::array<uint8_t,7> glyph(char value) {
  switch (value) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'J': return {7,2,2,2,2,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,25,21,19,19,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '.': return {0,0,0,0,0,6,6};
    case '-': return {0,0,0,31,0,0,0};
    case ':': return {0,6,6,0,6,6,0};
    default: return {};
  }
}
void fill(PixelSurface& surface, size_t left, size_t top, size_t width, size_t height, uint16_t color) {
  if (left >= surface.width || top >= surface.height) return;
  width = std::min(width, surface.width - left); height = std::min(height, surface.height - top);
  for (size_t y = 0; y < height; ++y)
    std::fill_n(surface.pixels + (top + y) * surface.stride + left, width, color);
}
void text(PixelSurface& surface, size_t left, size_t top, const char* value, unsigned scale, uint16_t color) {
  for (size_t letter = 0; value[letter] && letter < 64; ++letter) {
    const auto rows = glyph(value[letter]);
    for (size_t y = 0; y < 7; ++y)
      for (size_t x = 0; x < 5; ++x)
        if (rows[y] & (1U << (4 - x))) fill(surface, left + (letter * 6 + x) * scale, top + y * scale, scale, scale, color);
  }
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
