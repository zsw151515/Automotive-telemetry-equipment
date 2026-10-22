#include "test_support.h"
#include "ThermalView.h"
using namespace wired_tire;
static SnapshotDelivery delivery(uint32_t age, uint32_t elapsed = 0) {
  SnapshotDelivery result;
  result.snapshot.meta.wheel = WheelId::FL;
  result.snapshot.meta.state = FrameState::Valid;
  result.snapshot.meta.bootId = 1;
  result.snapshot.meta.receivedUs = 999999999999ULL; // unrelated MCU clock
  result.snapshot.ageMs = age;
  result.requestElapsedMs = elapsed;
  result.receivedLocalUs = 5000000;
  return result;
}
int main() {
  runTest("data_age_includes_peer_age_and_time_since_receipt", [] {
    const auto value = delivery(1400);
    CHECK(updateView(value, 5100000).fresh);
    CHECK(!updateView(value, 5101000).fresh);
    CHECK(updateView(value, 5101000).ageMs == 1501);
  });
  runTest("request_time_is_not_lost_and_repeated_stale_reads_do_not_renew_data", [] {
    CHECK(!updateView(delivery(1400, 101), 5000000).fresh);
    CHECK(!updateView(delivery(1600), 5000000).fresh);
    auto reread = delivery(1600); reread.receivedLocalUs = 10000000;
    CHECK(!updateView(reread, 10000000).fresh);
  });
  runTest("peer_clock_is_not_subtracted_from_host_clock", [] {
    auto value = delivery(20);
    CHECK(updateView(value, 5001000).fresh);
    value.snapshot.meta.receivedUs = 0;
    CHECK(updateView(value, 5001000).ageMs == 21);
  });
  runTest("bad_status_backward_host_clock_and_age_overflow_fail_safe", [] {
    auto value = delivery(0);
    value.snapshot.meta.state = FrameState::CommError; CHECK(!updateView(value, 5000000).fresh);
    value = delivery(0); CHECK(!updateView(value, 4999999).fresh);
    value = delivery(0xFFFFFFFF, 0xFFFFFFFF);
    CHECK(updateView(value, 9000000).ageMs == 0xFFFFFFFF);
    CHECK(!updateView(value, 9000000).fresh);
  });
  runTest("independent_wheels_and_restart_metadata", [] {
    auto a = delivery(0), b = delivery(2000);
    b.snapshot.meta.wheel = WheelId::RR;
    CHECK(updateView(a, 5000000).fresh);
    CHECK(!updateView(b, 5000000).fresh);
    a.snapshot.meta.bootId = 2; a.snapshot.meta.rxSequence = 0;
    CHECK(updateView(a, 5000000).fresh);
  });
  std::cout << "PASS wired_tire view\n";
}
