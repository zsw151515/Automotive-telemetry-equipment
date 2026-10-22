#include "test_support.h"
#include "RaceChronoBle.h"
using namespace wired_tire;
int main() {
  runTest("expired_half_batch_recovers_without_disconnect", [] {
    testNotifications.clear();
    RaceChronoBle ble;
    CHECK(ble.begin(WheelId::FL, LegacyAuxFields{}, true));
    ble.onConnect(nullptr);
    FrameMeta meta; meta.wheel = WheelId::FL; meta.bootId = 1; meta.state = FrameState::Valid;
    ZoneResult zones; zones.validMask = 0xFF; zones.temp10.fill(600);
    ble.poll(0, meta, zones); ble.poll(800000, meta, zones);
    CHECK(ble.notificationAttempts() == 1 && testNotifications.back() == 1);
    meta.receivedUs = 2400000;
    ble.poll(2400000, meta, zones);
    for (unsigned i = 1; i <= 4; ++i) ble.poll(2400000 + i * 1000, meta, zones);
    CHECK(ble.connected());
    CHECK(testNotifications.size() == 4);
    CHECK(testNotifications[1] == 1 && testNotifications[2] == 2 && testNotifications[3] == 3);
  });
  std::cout << "PASS wired_tire ble_driver\n";
}
