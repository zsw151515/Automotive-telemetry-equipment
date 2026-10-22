#include "ProbeLink.h"
#include <driver/uart.h>
#include <esp_system.h>
using namespace wired_tire;
bool ProbeLink::begin(WheelId wheel, int rx, int tx, int direction) {
  if (!validWheel(wheel) || rx < 0 || tx < 0 || direction < 0 ||
      rx == tx || rx == direction || tx == direction) return false;
  meta_.wheel = wheel;
  meta_.bootId = esp_random();
  if (!meta_.bootId) meta_.bootId = 1;
  config_.mirror = wheel == WheelId::FR || wheel == WheelId::RR;
  Serial1.begin(115200, SERIAL_8N2, rx, tx);
  if (uart_set_pin(UART_NUM_1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, direction, UART_PIN_NO_CHANGE) != ESP_OK ||
      uart_set_mode(UART_NUM_1, UART_MODE_RS485_HALF_DUPLEX) != ESP_OK) {
    Serial1.end(); return false;
  }
  enabled_ = true;
  Serial.println("RS485 initialized: 115200 8N2, probe 01, read-only 65");
  return true;
}
void ProbeLink::finish(uint64_t nowUs) {
  const auto parsed = decodeYf65(response_, count_, 1, candidate_);
  receiving_ = false;
  const bool accepted = schedule_.response(nowUs, parsed == ParseResult::Ok);
  if (!accepted) {
    if (parsed == ParseResult::CrcMismatch) ++stats_.crcErrors;
    else if (parsed != ParseResult::Ok) ++stats_.lengthErrors;
    ++stats_.dropped;
    if (!haveFrame_) meta_.state = FrameState::CommError;
    return;
  }
  const bool recovered = haveFrame_ && !isFresh(nowUs, meta_.receivedUs);
  latest_ = candidate_;
  zones_ = updateZones(latest_, nowUs, config_, filter_);
  ++meta_.rxSequence;
  meta_.receivedUs = nowUs;
  bool allValid = true;
  for (auto mask : latest_.valid) allValid = allValid && mask == 0xFF;
  meta_.state = allValid ? FrameState::Valid : FrameState::Partial;
  if (recovered) ++stats_.recoveries;
  if (!haveFrame_ || recovered) Serial.println("Probe valid response received (not proof of new exposure)");
  haveFrame_ = true;
}
void ProbeLink::poll(uint64_t nowUs) {
  if (!enabled_) return;
  if (haveFrame_ && !isFresh(nowUs, meta_.receivedUs)) meta_.state = FrameState::Stale;
  // Expire before consuming buffered bytes: a late tail must never renew freshness.
  const auto action = schedule_.poll(nowUs);
  if (action == ProbeAction::Timeout) {
    receiving_ = false; count_ = 0; ++stats_.timeouts;
    if (!haveFrame_) meta_.state = FrameState::CommError;
  }
  size_t budget = 512;
  while (Serial1.available() && budget--) {
    const int value = Serial1.read();
    if (value < 0) break;
    if (!receiving_) { ++stats_.dropped; continue; }
    if (count_ >= sizeof(response_)) {
      ++stats_.lengthErrors; ++stats_.dropped;
      receiving_ = false; schedule_.response(nowUs, false); break;
    }
    response_[count_++] = static_cast<uint8_t>(value);
    const size_t expected = expectedYfResponseBytes(response_, count_);
    if (expected) {
      if (expected > sizeof(response_)) {
        ++stats_.lengthErrors; receiving_ = false; schedule_.response(nowUs, false); break;
      }
      if (count_ >= expected) { finish(nowUs); break; }
    }
  }
  if (action == ProbeAction::Request) {
    // Keep the drain bounded. Retry later if old traffic still fills the FIFO.
    if (Serial1.available()) { schedule_.response(nowUs, false); ++stats_.dropped; return; }
    count_ = 0; receiving_ = true;
    uint8_t request[]{1, 0x65, 0, 0, 3, 0x11, 0, 0};
    const auto crc = modbusCrc(request, 6);
    request[6] = static_cast<uint8_t>(crc); request[7] = static_cast<uint8_t>(crc >> 8);
    if (Serial1.write(request, sizeof(request)) != sizeof(request)) {
      receiving_ = false; ++stats_.dropped; schedule_.response(nowUs, false);
    }
  }
}
bool ProbeLink::copyLatest(ThermalFrame& frame, FrameMeta& meta) const {
  if (!haveFrame_) return false;
  frame = latest_; meta = meta_; return true;
}
