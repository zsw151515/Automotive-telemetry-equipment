#include "test_support.h"
#include "SnapshotWire.h"
using namespace wired_tire;
static std::vector<uint8_t> encoded(uint32_t sequence = 42, uint32_t boot = 0x12345678, uint32_t age = 123) {
  ThermalFrame frame; frame.valid.fill(0xFF);
  for (size_t i = 0; i < kPixels; ++i) frame.temp10[i] = static_cast<int16_t>(static_cast<int>(i) - 384);
  FrameMeta meta; meta.wheel = WheelId::FL; meta.state = FrameState::Valid;
  meta.bootId = boot; meta.rxSequence = sequence; meta.receivedUs = 0x1122334455667788ULL;
  meta.bleEnabled = true; meta.bleConnected = true;
  ZoneResult zones; zones.validMask = 0xFF; zones.temp10.fill(100);
  LinkStats stats; stats.timeouts = 9;
  std::vector<uint8_t> wire(1728);
  CHECK(encodeSnapshot(frame, meta, zones, stats, age, 0xAABBCCDD, wire.data(), wire.size()) == EncodeResult::Ok);
  return wire;
}
static void fixCrc(std::vector<uint8_t>& wire) {
  const auto crc = wireCrc32(wire.data(), 1712);
  for (unsigned i = 0; i < 4; ++i) wire[1712 + i] = static_cast<uint8_t>(crc >> (8 * i));
}
static void reject(std::vector<uint8_t> wire, DecodeResult expected, WheelId wheel = WheelId::FL, uint32_t token = 0xAABBCCDD) {
  Snapshot previous; previous.meta.bootId = 777; previous.frame.temp10.fill(555);
  CHECK(decodeSnapshot(wire.data(), wire.size(), wheel, token, previous) == expected);
  CHECK(previous.meta.bootId == 777);
  for (auto temperature : previous.frame.temp10) CHECK(temperature == 555);
}
int main() {
  runTest("crc32_ieee_known_vector", [] {
    CHECK(wireCrc32(reinterpret_cast<const uint8_t*>("123456789"), 9) == 0xCBF43926);
  });
  runTest("wire_offsets_sign_and_link_status_round_trip", [] {
    const auto wire = encoded();
    CHECK(wire[0] == 'W' && wire[3] == '1' && wire[4] == 1 && wire[5] == 1);
    CHECK(wire[8] == 0x78 && wire[11] == 0x12);
    CHECK(wire[16] == 0x88 && wire[23] == 0x11);
    CHECK(wire[28] == 0x70 && wire[29] == 6);
    CHECK(wire[31] == 6); // BLE connected + enabled, not mirrored
    CHECK(wire[56] == 0xDD && wire[59] == 0xAA);
    CHECK(wire[64] == 0x80 && wire[65] == 0xFE); // -384
    CHECK(wire[1598] == 0x7F && wire[1599] == 1); // 383
    for (size_t i = 1716; i < 1728; ++i) CHECK(wire[i] == 0);
    Snapshot snapshot;
    CHECK(decodeSnapshot(wire.data(), wire.size(), WheelId::FL, 0xAABBCCDD, snapshot) == DecodeResult::Ok);
    CHECK(snapshot.frame.temp10[0] == -384 && snapshot.frame.temp10[767] == 383);
    CHECK(snapshot.meta.bleEnabled && snapshot.meta.bleConnected);
    CHECK(snapshot.stats.timeouts == 9 && snapshot.ageMs == 123);
  });
  runTest("crc_wrong_wheel_and_old_request_are_rejected_without_overwrite", [] {
    auto wire = encoded(); wire[100] ^= 1; reject(wire, DecodeResult::CrcMismatch);
    reject(encoded(), DecodeResult::WrongWheel, WheelId::RR);
    reject(encoded(), DecodeResult::WrongToken, WheelId::FL, 42);
  });
  runTest("unknown_version_bad_length_reserved_and_semantic_values", [] {
    auto wire = encoded(); wire[4] = 2; fixCrc(wire); reject(wire, DecodeResult::UnsupportedVersion);
    wire = encoded(); wire.resize(1727); reject(wire, DecodeResult::LengthMismatch);
    wire = encoded(); wire[60] = 1; fixCrc(wire); reject(wire, DecodeResult::InvalidFields);
    wire = encoded(); wire[1716] = 1; reject(wire, DecodeResult::InvalidFields);
    wire = encoded(); wire[30] = 17; fixCrc(wire); reject(wire, DecodeResult::InvalidFields);
    wire = encoded(); wire[31] = 0x80; fixCrc(wire); reject(wire, DecodeResult::InvalidFields);
    wire = encoded(); wire[6] = 255; fixCrc(wire); reject(wire, DecodeResult::InvalidFields);
  });
  runTest("sequence_wrap_reboot_and_aging_are_metadata_not_new_exposure", [] {
    Snapshot a, b, c;
    auto first = encoded(0xFFFFFFFF, 1, 0), wrapped = encoded(0, 1, 300), rebooted = encoded(0, 2, 1000);
    CHECK(decodeSnapshot(first.data(), first.size(), WheelId::FL, 0xAABBCCDD, a) == DecodeResult::Ok);
    CHECK(decodeSnapshot(wrapped.data(), wrapped.size(), WheelId::FL, 0xAABBCCDD, b) == DecodeResult::Ok);
    CHECK(decodeSnapshot(rebooted.data(), rebooted.size(), WheelId::FL, 0xAABBCCDD, c) == DecodeResult::Ok);
    CHECK(a.frame.temp10 == b.frame.temp10 && b.frame.temp10 == c.frame.temp10);
    CHECK(b.meta.rxSequence == 0 && c.meta.bootId == 2 && c.ageMs == 1000);
  });
  std::cout << "PASS wired_tire snapshot\n";
}
