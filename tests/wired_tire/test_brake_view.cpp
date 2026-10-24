#include "brake_test_support.h"
#include "BrakeWire.h"
int main(){
 runTest("state_age_900_plus_request_101_is_stale",[]{BrakeDelivery d{};d.state=validBrakeTelemetry();d.state.stateAgeMs=900;d.receivedLocalUs=1000000;
  CHECK(updateBrakeView(d,1000000).stateFresh);d.requestElapsedMs=101;CHECK(!updateBrakeView(d,1000000).stateFresh);});
 runTest("sensor_age_2400_plus_host_101_is_invalid",[]{BrakeDelivery d{};d.state=validBrakeTelemetry();d.state.sensorAgeMs={2400,0};d.receivedLocalUs=1000000;
  auto v=updateBrakeView(d,1101000);CHECK(!v.sensorFresh[0]&&v.sensorFresh[1]&&v.stateFresh);CHECK(!updateBrakeView(d,999999).stateFresh);});
 runTest("repeated_reads_and_peer_reboot_do_not_rejuvenate",[]{BrakeDelivery d{};d.state=validBrakeTelemetry();d.state.stateAgeMs=2000;d.state.sensorAgeMs={UINT32_MAX,UINT32_MAX};
  for(uint64_t t:{0ull,1000000ull,2000000ull}){d.receivedLocalUs=t;auto v=updateBrakeView(d,t);CHECK(!v.stateFresh&&!v.sensorFresh[0]);}
  d.state.brakeBootId=2;CHECK(!updateBrakeView(d,2000000).stateFresh);});
}
