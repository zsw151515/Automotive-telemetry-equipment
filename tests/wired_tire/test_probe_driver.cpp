#include "test_support.h"
#include "ProbeLink.h"
using namespace wired_tire;
static void start(ProbeLink& probe) {
  Serial1 = TestSerial{};
  CHECK(probe.begin(WheelId::FL, 1, 2, 3));
  probe.poll(0);
  CHECK(Serial1.tx.size() == 8);
}
int main() {
  runTest("late_tail_cannot_publish_probe_frame_or_renew_freshness", [] {
    ProbeLink probe; start(probe);
    Serial1.inject(yfFixture());
    probe.poll(100000); probe.poll(100001); probe.poll(100002);
    CHECK(probe.meta().rxSequence == 0 && Serial1.available() == 40);
    probe.poll(1700000);
    CHECK(probe.meta().rxSequence == 0 && probe.stats().timeouts == 1);
    ThermalFrame frame; FrameMeta meta;
    CHECK(!probe.copyLatest(frame, meta));
  });
  runTest("late_tail_preserves_last_good_frame_at_exact_deadline", [] {
    ProbeLink probe; start(probe);
    Serial1.inject(yfFixture());
    for (unsigned i = 0; i < 4; ++i) probe.poll(150000 + i);
    CHECK(probe.meta().rxSequence == 1 && probe.meta().receivedUs == 150003);
    probe.poll(250000);
    Serial1.inject(yfFixture());
    probe.poll(400000); probe.poll(400001); probe.poll(400002);
    probe.poll(650000);
    CHECK(probe.meta().rxSequence == 1 && probe.meta().receivedUs == 150003);
    CHECK(probe.stats().timeouts == 1);
    probe.poll(1150000);
    Serial1.inject(yfFixture());
    for (unsigned i = 0; i < 4; ++i) probe.poll(1300000 + i);
    CHECK(probe.meta().rxSequence == 2 && probe.meta().receivedUs == 1300003);
  });
  runTest("unsolicited_fifo_is_drained_before_starting_new_request", [] {
    ProbeLink probe; start(probe); probe.poll(400000);
    Serial1.inject(yfFixture());
    probe.poll(900000);
    CHECK(Serial1.tx.size() == 8); // Bounded drain must not issue a request into old data.
    for (unsigned i = 0; i < 4; ++i) probe.poll(900001 + i);
    CHECK(probe.meta().rxSequence == 0 && Serial1.available() == 0);
    probe.poll(1400000);
    CHECK(Serial1.tx.size() == 16);
  });
  std::cout << "PASS wired_tire probe_driver\n";
}
