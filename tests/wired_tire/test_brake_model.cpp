#include "test_support.h"
#include "BrakeModel.h"
using namespace wired_brake;
int main(){
 runTest("zero_time_core_hold_6s",[]{BrakeModel m;std::array<BrakeReading,2> r{};
  for(uint64_t t=0;t<=6000000;t+=100000){r[0]=r[1]={6100,true,t};CHECK((m.tick(r,3,{},t).state==BrakeState::Alarm)==(t==6000000));}});
 runTest("surface_auxiliary_hold_5s",[]{BrakeModel m;std::array<BrakeReading,2> r{};
  for(uint64_t t=0;t<=5000000;t+=100000){r[0]=r[1]={6500,true,t};CHECK((m.tick(r,3,{},t).state==BrakeState::Alarm)==(t==5000000));}});
 runTest("alarm_beats_partial_fail_and_all_fail_resets_model",[]{BrakeModel m;std::array<BrakeReading,2> r{};
  for(uint64_t t=0;t<=6000000;t+=100000){r[0]={6100,true,t};m.tick(r,3,{},t);}
  CHECK(m.result().state==BrakeState::Alarm&&m.result().failMask==2);
  r[0].valid=false;const auto x=m.tick(r,3,{},6100000);CHECK(x.state==BrakeState::Fail&&!x.modelValid&&x.core10==0x7fff);});
 runTest("clock_boundary_long_gap_reinitializes",[]{BrakeModel m;std::array<BrakeReading,2> r{};const uint64_t base=uint64_t(UINT32_MAX)*1000;
  for(uint64_t d=0;d<=5900000;d+=100000){r[0]={6100,true,base+d};m.tick(r,1,{},base+d);}
  r[0].receivedUs=base+10000000;CHECK(m.tick(r,1,{},base+10000000).state!=BrakeState::Alarm);
  r[0]={0,true,0};CHECK(m.tick(r,1,{},0).core10==0);});
 runTest("filter_dose_release_and_normal_reference",[]{BrakeModel m;std::array<BrakeReading,2> r{};r[0]={0,true,0};m.tick(r,1,{},0);
  r[0]={9000,true,100000};auto x=m.tick(r,1,{},100000);CHECK(x.fast10==1000&&x.core10==45);
  r[0]={-400,true,200000};CHECK(m.tick(r,1,{},200000).modelValid);
  r[0]={12001,true,300000};CHECK(m.tick(r,1,{},300000).state==BrakeState::Fail);});
}
