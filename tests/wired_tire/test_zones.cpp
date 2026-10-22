#include "test_support.h"
#include "TireZones.h"
#include <cmath>
#include <limits>
using namespace wired_tire;
static ThermalFrame flat(int16_t temperature) {
  ThermalFrame frame; frame.temp10.fill(temperature); frame.valid.fill(0xFF); return frame;
}
int main() {
  runTest("valid_zero_is_not_failure", [] {
    auto frame = flat(0); ZoneConfig config; ZoneFilterState state;
    const auto result = updateZones(frame, 250000, config, state);
    CHECK(result.validMask == 0xFF);
    for (auto value : result.temp10) CHECK(value == 0);
  });
  runTest("eight_four_column_zones_and_mirror", [] {
    auto frame = flat(0);
    for (size_t i = 0; i < kPixels; ++i) frame.temp10[i] = static_cast<int16_t>((i % 32 / 4) * 100);
    const auto original = frame; ZoneConfig config; ZoneFilterState state;
    auto result = updateZones(frame, 250000, config, state);
    CHECK(result.validMask == 0xFF);
    for (size_t zone = 0; zone < 8; ++zone) CHECK(result.temp10[zone] == static_cast<int16_t>(zone * 100));
    config.mirror = true; result = updateZones(frame, 500000, config, state);
    for (size_t zone = 0; zone < 8; ++zone) CHECK(result.temp10[zone] == static_cast<int16_t>((7 - zone) * 100));
    CHECK(frame.temp10 == original.temp10 && frame.valid == original.valid);
  });
  runTest("invalid_zone_is_not_zero_filled_as_valid", [] {
    auto frame = flat(300);
    for (size_t row = 0; row < kRows; ++row)
      for (size_t col = 0; col < 4; ++col) frame.setValid(row * 32 + col, false);
    ZoneConfig config; ZoneFilterState state;
    const auto result = updateZones(frame, 250000, config, state);
    CHECK(result.validMask == 0xFE);
  });
  runTest("invalid_and_out_of_algorithm_range", [] {
    ZoneConfig config; ZoneFilterState state; ThermalFrame frame;
    CHECK(updateZones(frame, 250000, config, state).validMask == 0);
    frame = flat(2100);
    CHECK(updateZones(frame, 500000, config, state).validMask == 0);
  });
  runTest("isolated_states_and_repeated_static_image", [] {
    ZoneConfig config; ZoneFilterState a, b;
    auto frameA = flat(400), frameB = flat(800);
    CHECK(updateZones(frameA, 250000, config, a).temp10[0] == 400);
    CHECK(updateZones(frameB, 250000, config, b).temp10[0] == 800);
    CHECK(updateZones(frameA, 500000, config, a).temp10[0] == 400);
    CHECK(updateZones(frameB, 500000, config, b).temp10[0] == 800);
  });
  runTest("duplicate_or_backward_time_not_published", [] {
    ZoneConfig config; ZoneFilterState state; auto frame = flat(400);
    updateZones(frame, 1000000, config, state);
    CHECK(updateZones(frame, 1000000, config, state).validMask == 0);
    CHECK(updateZones(frame, 999999, config, state).validMask == 0);
    CHECK(state.lastFrameUs == 1000000);
  });
  runTest("stale_gap_reinitializes_filter", [] {
    ZoneConfig config; ZoneFilterState state;
    updateZones(flat(0), 250000, config, state);
    CHECK(updateZones(flat(900), 2000000, config, state).temp10[0] == 900);
  });
  runTest("uses_real_elapsed_time_not_200ms_clamp", [] {
    ZoneConfig config; config.maxRateCPerSecond = 1; config.deadbandC = 0;
    ZoneFilterState state;
    updateZones(flat(0), 1000000, config, state);
    const auto value = updateZones(flat(1000), 2000000, config, state).temp10[0];
    CHECK(value >= 9 && value <= 10);
  });
  runTest("nan_config_rejected_and_corrupt_state_recovered", [] {
    ZoneConfig config; ZoneFilterState state;
    updateZones(flat(300), 250000, config, state);
    state.filtered[0] = std::numeric_limits<float>::quiet_NaN();
    CHECK(updateZones(flat(400), 500000, config, state).temp10[0] == 400);
    CHECK(std::isfinite(state.filtered[0]));
    config.maxRateCPerSecond = std::numeric_limits<float>::quiet_NaN();
    CHECK(updateZones(flat(400), 750000, config, state).validMask == 0);
  });
  runTest("roi_clamped_to_last_complete_eight_rows", [] {
    auto frame = flat(0);
    for (size_t row = 16; row < 24; ++row)
      for (size_t col = 0; col < 32; ++col) frame.temp10[row * 32 + col] = 1000;
    ZoneConfig config; ZoneFilterState state;
    const auto result = updateZones(frame, 250000, config, state);
    CHECK(result.roiRow == 16 && result.validMask == 0xFF);
  });
  std::cout << "PASS wired_tire zones\n";
}
