#include "TireZones.h"
#include <algorithm>
#include <cmath>
#include <limits>
namespace wired_tire {
namespace {
bool usable(const ThermalFrame& frame, size_t i) {
  return frame.isValid(i) && frame.temp10[i] >= -200 && frame.temp10[i] <= 2000;
}
float trimmed(float* values, size_t count) {
  if (!count) return 0;
  std::sort(values, values + count);
  const size_t trim = std::min<size_t>(3, (count - 1) / 2);
  float sum = 0;
  for (size_t i = trim; i < count - trim; ++i) sum += values[i];
  return sum / static_cast<float>(count - 2 * trim);
}
float timeAlpha(float reference, float seconds) {
  return 1.0f - std::pow(1.0f - reference, seconds / 0.034f);
}
}
ZoneResult updateZones(const ThermalFrame& frame, uint64_t rxUs,
                       const ZoneConfig& config, ZoneFilterState& state) {
  ZoneResult result;
  result.mirrored = config.mirror;
  result.configVersion = config.version;
  if (!std::isfinite(config.maxRateCPerSecond) || config.maxRateCPerSecond <= 0 ||
      !std::isfinite(config.deadbandC) || config.deadbandC < 0 ||
      !std::isfinite(config.spikeGateC) || config.spikeGateC <= 0) return result;
  if (state.hasFrame && rxUs <= state.lastFrameUs) return result;
  const bool reset = !state.hasFrame || state.configVersion != config.version ||
      state.mirror != config.mirror || rxUs - state.lastFrameUs > 1500000 || !std::isfinite(state.roi);
  const float frameDt = reset ? 0.034f : static_cast<float>(rxUs - state.lastFrameUs) / 1000000.0f;
  if (reset) {
    state = ZoneFilterState{};
    state.configVersion = config.version;
    state.mirror = config.mirror;
  }
  float scores[kRows]{};
  unsigned counts[kRows]{};
  for (size_t row = 0; row < kRows; ++row) {
    float samples[kCols]; size_t count = 0;
    for (size_t col = 0; col < kCols; ++col) {
      const size_t index = row * kCols + col;
      if (usable(frame, index)) samples[count++] = frame.temp10[index] / 10.0f;
    }
    scores[row] = trimmed(samples, count);
    counts[row] = static_cast<unsigned>(count);
  }
  int best = static_cast<int>(std::lround(std::clamp(state.roi, 3.0f, 16.0f)));
  float bestScore = -std::numeric_limits<float>::infinity();
  // 17 is the legacy search setting; an 8-row ROI in 24 rows can start no later than 16.
  for (int start = 3; start <= 16; ++start) {
    float sum = 0; unsigned rows = 0;
    for (int row = start; row < start + 8; ++row)
      if (counts[row]) { sum += scores[row]; ++rows; }
    if (rows < 4) continue;
    const float score = sum / static_cast<float>(rows) - std::fabs(start - state.roi) * 0.16f;
    if (score > bestScore) { bestScore = score; best = start; }
  }
  if (reset) state.roi = static_cast<float>(best);
  else {
    const float maxRows = frameDt / 0.034f;
    const float target = std::clamp(static_cast<float>(best), state.roi - maxRows, state.roi + maxRows);
    state.roi += (target - state.roi) * timeAlpha(0.35f, frameDt);
  }
  const int roi = static_cast<int>(std::lround(std::clamp(state.roi, 3.0f, 16.0f)));
  result.roiRow = static_cast<uint8_t>(roi);
  for (size_t zone = 0; zone < kZones; ++zone) {
    const size_t source = config.mirror ? kZones - 1 - zone : zone;
    float samples[32]; size_t count = 0;
    for (int row = roi; row < roi + 8; ++row)
      for (size_t col = source * 4; col < source * 4 + 4; ++col) {
        const size_t index = static_cast<size_t>(row) * kCols + col;
        if (usable(frame, index)) samples[count++] = frame.temp10[index] / 10.0f;
      }
    if (count < 18) continue;
    const float raw = trimmed(samples, count);
    const auto bit = static_cast<uint8_t>(1U << zone);
    const bool initialize = !(state.initializedMask & bit) || !std::isfinite(state.filtered[zone]) ||
        !std::isfinite(state.lastRaw[zone]) || rxUs - state.lastZoneUs[zone] > 1500000;
    if (initialize) state.filtered[zone] = raw;
    else {
      const float dt = static_cast<float>(rxUs - state.lastZoneUs[zone]) / 1000000.0f;
      const float maxStep = config.maxRateCPerSecond * dt;
      const float target = std::clamp(raw, state.filtered[zone] - maxStep, state.filtered[zone] + maxStep);
      const float delta = std::fabs(target - state.filtered[zone]);
      if (delta > config.deadbandC) {
        float reference = delta < 0.15f ? 0.10f : delta < 0.70f ? 0.24f : delta < 2.0f ? 0.45f : 0.72f;
        if (std::fabs(raw - state.lastRaw[zone]) > config.spikeGateC &&
            std::fabs(raw - state.filtered[zone]) > config.spikeGateC)
          reference = std::min(reference, 0.45f);
        state.filtered[zone] += (target - state.filtered[zone]) * timeAlpha(reference, dt);
      }
    }
    if (!std::isfinite(state.filtered[zone])) continue;
    const auto value = std::lround(std::clamp(state.filtered[zone], -20.0f, 200.0f) * 10);
    result.temp10[zone] = static_cast<int16_t>(value);
    result.validMask |= bit;
    state.lastRaw[zone] = raw;
    state.lastZoneUs[zone] = rxUs;
    state.initializedMask |= bit;
  }
  state.lastFrameUs = rxUs;
  state.hasFrame = true;
  return result;
}
}
