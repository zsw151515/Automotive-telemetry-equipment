#include "test_support.h"
#include "ThermalView.h"
#include <fstream>
#include <limits>
using namespace wired_tire;
static ThermalFrame flat(int16_t value) {
  ThermalFrame frame; frame.temp10.fill(value); frame.valid.fill(0xFF); return frame;
}
static std::array<SnapshotDelivery,4> samples() {
  std::array<SnapshotDelivery,4> values;
  for (unsigned i = 0; i < 4; ++i) {
    values[i].snapshot.frame = flat(600);
    values[i].snapshot.meta.wheel = static_cast<WheelId>(i + 1);
    values[i].snapshot.meta.bootId = i + 1;
    values[i].snapshot.meta.state = FrameState::Valid;
    values[i].snapshot.meta.bleEnabled = values[i].snapshot.meta.bleConnected = true;
    values[i].snapshot.ageMs = 0;
  }
  return values;
}
int main(int argc, char** argv) {
  runTest("dashboard_mirrors_columns_and_validity_to_match_all_four_ble_zone_orders", [] {
    auto values = samples(); ColorScale scale;
    std::vector<uint16_t> pixels(1024 * 600);
    PixelSurface surface{pixels.data(),1024,600,1024,pixels.size()};
    const size_t origins[]{56 * 1024 + 12,56 * 1024 + 524,336 * 1024 + 12,336 * 1024 + 524};
    for (size_t w = 0; w < 4; ++w) {
      auto& frame = values[w].snapshot.frame;
      for (size_t row = 0; row < 24; ++row)
        for (size_t col = 0; col < 32; ++col) frame.temp10[row * 32 + col] = static_cast<int16_t>(300 + col * 30);
      frame.setValid(31, false);
      values[w].snapshot.zones.mirrored = w % 2 != 0;
    }
    renderDashboard(values, 1000, scale, surface);
    for (size_t w = 0; w < 4; ++w) {
      const auto& frame = values[w].snapshot.frame;
      const size_t first = w % 2 ? 31 : 0, last = w % 2 ? 0 : 31;
      CHECK(surface.pixels[origins[w]] == heatColor(frame.temp10[first], frame.isValid(first), scale));
      CHECK(surface.pixels[origins[w] + 319] == heatColor(frame.temp10[last], frame.isValid(last), scale));
      CHECK(frame.temp10[0] == 300 && frame.temp10[31] == 1230 && !frame.isValid(31));
    }
  });
  runTest("refresh_fence_keeps_scanout_buffers_leased_across_refreshes", [] {
    DisplaySwapFence fence;
    CHECK(fence.available(10) && fence.drawIndex() == 1);
    CHECK(fence.presented(10));
    CHECK(!fence.available(10) && !fence.available(11));
    CHECK(!fence.presented(11));
    CHECK(fence.available(12) && fence.drawIndex() == 0);
    CHECK(fence.presented(0xFFFFFFFF));
    CHECK(!fence.available(0) && fence.available(1));
  });
  runTest("noninteger_scaling_uses_every_source_boundary_correctly", [] {
    auto frame = flat(0); ColorScale scale;
    for (size_t i = 0; i < 768; ++i) frame.temp10[i] = static_cast<int16_t>(200 + (i % 32) * 35);
    std::vector<uint16_t> pixels(33 * 24);
    PixelSurface surface{pixels.data(),33,24,33,pixels.size()};
    renderHeatmap(frame, scale, surface);
    CHECK(pixels[0] == heatColor(200, true, scale));
    CHECK(pixels[1] == heatColor(200, true, scale));
    CHECK(pixels[2] == heatColor(235, true, scale));
    CHECK(pixels[32] == heatColor(1285, true, scale));
  });
  runTest("all_768_pixels_mapped_without_mutation_or_out_of_bounds", [] {
    const uint16_t guard = 0xA55A;
    std::vector<uint16_t> pixels(770, guard);
    PixelSurface surface{pixels.data() + 1, 32, 24, 32, 768};
    auto frame = flat(0); ColorScale scale;
    for (size_t i = 0; i < 768; ++i) frame.temp10[i] = static_cast<int16_t>(200 + (i * 37) % 1201);
    const auto original = frame.temp10;
    renderHeatmap(frame, scale, surface);
    for (size_t i = 0; i < 768; ++i) CHECK(pixels[i + 1] == heatColor(frame.temp10[i], true, scale));
    CHECK(pixels.front() == guard && pixels.back() == guard && frame.temp10 == original);
  });
  runTest("invalid_distinct_from_zero_and_scale_clamping", [] {
    ColorScale scale;
    CHECK(heatColor(0, true, scale) != heatColor(0, false, scale));
    CHECK(heatColor(-400, true, scale) == heatColor(200, true, scale));
    CHECK(heatColor(3000, true, scale) == heatColor(1400, true, scale));
    CHECK(heatColor(200, true, scale) != heatColor(1400, true, scale));
    CHECK(heatColor(32767, true, scale) == heatColor(0, false, scale));
  });
  runTest("maximum_excludes_invalid_and_handles_negative_and_empty_frames", [] {
    auto frame = flat(-100); frame.temp10[0] = 3000; frame.setValid(0, false);
    int16_t result = 999;
    CHECK(maximumTemperature(frame, result) && result == -100);
    frame.valid.fill(0); CHECK(!maximumTemperature(frame, result)); CHECK(result == -100);
  });
  runTest("invalid_stride_capacity_and_overflow_do_not_write", [] {
    uint16_t pixels[32]; for (auto& value : pixels) value = 0xA55A;
    auto frame = flat(600); ColorScale scale;
    PixelSurface surface{pixels, 32, 24, 31, 32}; renderHeatmap(frame, scale, surface);
    surface = {pixels, 32, 24, 32, 32}; renderHeatmap(frame, scale, surface);
    surface = {pixels, 32, std::numeric_limits<size_t>::max(), 32, 32}; renderHeatmap(frame, scale, surface);
    for (auto value : pixels) CHECK(value == 0xA55A);
  });
  runTest("four_dashboard_quadrants_share_color_scale_and_stale_is_grey", [] {
    std::vector<uint16_t> pixels(1024 * 600 + 2, 0xA55A);
    PixelSurface surface{pixels.data() + 1, 1024, 600, 1024, 1024 * 600};
    auto values = samples(); ColorScale scale;
    renderDashboard(values, 1000, scale, surface, false);
    const size_t origins[]{56 * 1024 + 12, 56 * 1024 + 524, 336 * 1024 + 12, 336 * 1024 + 524};
    for (auto origin : origins) CHECK(surface.pixels[origin] == heatColor(600, true, scale));
    values[3].snapshot.ageMs = 2000;
    renderDashboard(values, 1000, scale, surface, false);
    CHECK(surface.pixels[origins[3]] != heatColor(600, true, scale));
    CHECK(pixels.front() == 0xA55A && pixels.back() == 0xA55A);
  });
  if (argc == 3 && std::string(argv[1]) == "--preview") {
    auto values = samples();
    for (unsigned wheel = 0; wheel < 4; ++wheel)
      for (size_t row = 0; row < 24; ++row)
        for (size_t col = 0; col < 32; ++col) {
          const int centre = 9 + static_cast<int>(wheel) * 4;
          const int distance = std::abs(static_cast<int>(col) - centre);
          values[wheel].snapshot.frame.temp10[row * 32 + col] =
              static_cast<int16_t>(350 + std::max(0, 700 - distance * 55) + static_cast<int>(row) * 4);
        }
    values[3].snapshot.meta.bleConnected = false;
    std::vector<uint16_t> pixels(1024 * 600);
    PixelSurface surface{pixels.data(),1024,600,1024,pixels.size()};
    renderDashboard(values, 1000, ColorScale{}, surface, true);
    std::ofstream image(argv[2], std::ios::binary);
    image << "P6\n1024 600\n255\n";
    for (auto pixel : pixels) {
      const char rgb[]{static_cast<char>(((pixel >> 11) & 31) * 255 / 31),
          static_cast<char>(((pixel >> 5) & 63) * 255 / 63), static_cast<char>((pixel & 31) * 255 / 31)};
      image.write(rgb, 3);
    }
    CHECK(image.good());
    std::cout << "PREVIEW synthetic data only: " << argv[2] << '\n';
  } else if (argc != 1) return 2;
  std::cout << "PASS wired_tire heatmap\n";
}
