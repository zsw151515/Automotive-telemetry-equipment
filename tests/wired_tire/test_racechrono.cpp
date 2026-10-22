#include "test_support.h"
#include "RaceChronoPacket.h"
using namespace wired_tire;
static ZoneResult validZones() {
  ZoneResult zones; zones.validMask = 0xFF;
  zones.temp10 = {-100, 0, 1, 255, 256, 500, 1000, 2000};
  return zones;
}
int main() {
  runTest("stable_names_distinguish_all_four_wheels_and_check_capacity", [] {
    const uint8_t mac[]{0, 1, 2, 0xA1, 0xB2, 0xC3};
    char name[20]{};
    const char* expected[]{"RejsaRubberFLA1B2C3", "RejsaRubberFRA1B2C3", "RejsaRubberRLA1B2C3", "RejsaRubberRRA1B2C3"};
    for (unsigned wheel = 1; wheel <= 4; ++wheel) {
      CHECK(rejsaName(static_cast<WheelId>(wheel), mac, name, sizeof(name)));
      CHECK(std::string(name) == expected[wheel - 1]);
    }
    name[0] = 'X';
    CHECK(!rejsaName(WheelId::FL, mac, name, 19));
    CHECK(name[0] == 'X');
    CHECK(!rejsaName(static_cast<WheelId>(0), mac, name, sizeof(name)));
  });
  runTest("packet_layout_has_exact_signed_little_endian_temperature_bytes", [] {
    auto zones = validZones(); LegacyAuxFields aux;
    aux.characteristic1 = {0, 0x12, 0x34}; aux.characteristic2 = {0x56, 0x78, 0x9A};
    uint8_t guarded[22]; for (auto& value : guarded) value = 0xA5;
    CHECK(buildRaceChronoPacket(1, zones, aux, guarded + 1));
    CHECK(guarded[0] == 0xA5 && guarded[21] == 0xA5);
    const uint8_t expected[]{2, 0, 0x12, 0x34, 0x9C, 0xFF, 0, 0, 1, 0, 0xFF, 0, 0, 1, 0xF4, 1, 0xE8, 3, 0xD0, 7};
    for (size_t i = 0; i < 20; ++i) CHECK(guarded[i + 1] == expected[i]);
    CHECK(buildRaceChronoPacket(2, zones, aux, guarded + 1));
    CHECK(guarded[2] == 0x56 && guarded[3] == 0x78 && guarded[4] == 0x9A);
  });
  runTest("invalid_or_unsupported_packets_do_not_overwrite_output", [] {
    auto zones = validZones(); LegacyAuxFields aux; uint8_t output[20];
    for (auto& value : output) value = 0xCC;
    zones.validMask = 0xFE; CHECK(!buildRaceChronoPacket(1, zones, aux, output));
    zones = validZones(); zones.temp10[0] = 32767;
    CHECK(!buildRaceChronoPacket(1, zones, aux, output));
    zones = validZones(); CHECK(!buildRaceChronoPacket(4, zones, aux, output));
    CHECK(!buildRaceChronoPacket(0, zones, aux, output));
    for (auto value : output) CHECK(value == 0xCC);
  });
  runTest("notification_requires_connected_fresh_valid_wheel_and_zones", [] {
    auto zones = validZones(); FrameMeta meta;
    meta.wheel = WheelId::FL; meta.state = FrameState::Valid; meta.receivedUs = 1000000;
    CHECK(canNotify(meta, zones, 2500000, true));
    CHECK(!canNotify(meta, zones, 2501000, true));
    CHECK(!canNotify(meta, zones, 2000000, false));
    meta.state = FrameState::Stale; CHECK(!canNotify(meta, zones, 2000000, true));
    meta.state = FrameState::Partial; CHECK(canNotify(meta, zones, 2000000, true));
    meta.wheel = static_cast<WheelId>(0); CHECK(!canNotify(meta, zones, 2000000, true));
    meta.wheel = WheelId::FL; zones.validMask = 0x7F;
    CHECK(!canNotify(meta, zones, 2000000, true));
  });
  std::cout << "PASS wired_tire racechrono\n";
}
