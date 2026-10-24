#include "test_support.h"
#include "BrakeBuzzer.h"
using namespace wired_brake;
int main(){
 runTest("startup_3s_and_warn_alarm_patterns",[]{BrakeBuzzer b;b.begin(0);
  CHECK(!b.output(BrakeState::Alarm,true,2999999));CHECK(b.output(BrakeState::Alarm,true,3000000));
  CHECK(!b.output(BrakeState::Alarm,true,3150000));CHECK(b.output(BrakeState::Alarm,true,3300000));
  CHECK(b.output(BrakeState::Warn,true,4500000));CHECK(!b.output(BrakeState::Warn,true,4580000));});
 runTest("mute_expires_at_60000000us_without_millis_wrap",[]{BrakeBuzzer b;b.begin(0);b.mute60(0);
  CHECK(b.remainingMs(0)==60000);CHECK(!b.output(BrakeState::Alarm,true,59999999));CHECK(b.output(BrakeState::Alarm,true,60000000));
  const uint64_t base=uint64_t(UINT32_MAX)*1000;b.mute60(base);CHECK(b.remainingMs(base+59999000)==1);CHECK(b.remainingMs(base+60000000)==0);});
 runTest("warning_off_does_not_disable_alarm_and_fail_is_silent",[]{BrakeBuzzer b;b.begin(0);
  CHECK(b.output(BrakeState::Alarm,false,3000000));CHECK(!b.output(BrakeState::Warn,false,4500000));CHECK(!b.output(BrakeState::Fail,true,6000000));});
}
