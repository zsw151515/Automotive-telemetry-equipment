#include "test_support.h"
#include "ProbeSchedule.h"
using namespace wired_tire;
int main() {
  runTest("freshness_boundary_and_clock_regression", [] {
    CHECK(isFresh(1500000, 0)); CHECK(!isFresh(1501000, 0));
    CHECK(!isFresh(100, 101));
  });
  runTest("missing_probe_never_blocks_or_reenters_request", [] {
    ProbeSchedule schedule;
    CHECK(schedule.poll(0) == ProbeAction::Request);
    CHECK(schedule.poll(100000) == ProbeAction::None);
    CHECK(schedule.poll(399999) == ProbeAction::None);
    CHECK(schedule.poll(400000) == ProbeAction::Timeout);
    CHECK(schedule.poll(400001) == ProbeAction::None);
    CHECK(schedule.poll(899999) == ProbeAction::None);
    CHECK(schedule.poll(900000) == ProbeAction::Request);
  });
  runTest("successful_response_respects_poll_interval", [] {
    ProbeSchedule schedule;
    CHECK(schedule.poll(1000000) == ProbeAction::Request);
    schedule.response(1150000, true);
    CHECK(schedule.poll(1249999) == ProbeAction::None);
    CHECK(schedule.poll(1250000) == ProbeAction::Request);
  });
  runTest("rejected_response_does_not_renew_last_good_timestamp", [] {
    ProbeSchedule schedule;
    schedule.poll(0); schedule.response(150000, true);
    CHECK(schedule.hasGoodResponse() && schedule.lastGoodUs() == 150000);
    schedule.poll(250000); schedule.response(300000, false);
    CHECK(schedule.lastGoodUs() == 150000);
    CHECK(schedule.poll(799999) == ProbeAction::None);
    CHECK(schedule.poll(800000) == ProbeAction::Request);
  });
  runTest("unrequested_responses_cannot_publish_freshness", [] {
    ProbeSchedule schedule;
    schedule.response(1000, true);
    CHECK(!schedule.hasGoodResponse());
  });
  runTest("backward_clock_clears_inflight_and_freshness", [] {
    ProbeSchedule schedule; schedule.poll(1000000); schedule.response(1100000, true);
    CHECK(schedule.poll(10) == ProbeAction::Request);
    CHECK(!schedule.hasGoodResponse());
  });
  std::cout << "PASS wired_tire schedule\n";
}
