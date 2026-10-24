#include "brake_test_support.h"
#include "BrakeCan.h"
int main(){
 runTest("eight_pid_bytes_negative_invalid_units",[]{auto s=validBrakeTelemetry();s.temp10[0]=-123;s.dose10=321;s.state=BrakeState::Alarm;uint8_t b[20]{};
  const uint16_t words[]{uint16_t(-123),6100,6100,6100,6100,321,2,600};
  for(unsigned i=0;i<8;++i){CHECK(encodeBrakeCan(0x100+i,s,b)==(i==6?5:6));CHECK(b[0]==i&&b[1]==1&&b[2]==0&&b[3]==0);
   if(i==6)CHECK(b[4]==2);else CHECK(b[4]==uint8_t(words[i]>>8)&&b[5]==uint8_t(words[i]));}
  s.validMask=2;CHECK(encodeBrakeCan(0x100,s,b)==6&&b[4]==0x7f&&b[5]==0xff);s.modelValid=false;encodeBrakeCan(0x104,s,b);CHECK(b[4]==0x7f&&b[5]==0xff);CHECK(!encodeBrakeCan(0x108,s,b));
 });
 runTest("deny_all_allow_all_custom_interval_dual_endian_zero",[]{BrakeCanFilter f;uint8_t deny[]{0};CHECK(f.due(0x100,0)&&!f.due(0x100,149999)&&f.due(0x100,150000));CHECK(f.apply(deny,1)&&!f.due(0x100,1000000));
  uint8_t allow[]{1,3,0xe8};CHECK(f.apply(allow,3));CHECK(f.due(0x100,0)&&!f.due(0x100,999999)&&f.due(0x100,1000000));
  uint8_t one[]{2,0,0,0,0,1,0};CHECK(f.apply(one,7));CHECK(f.due(0x100,0)&&f.due(0x10000,0)&&!f.due(0x101,0));CHECK(!f.due(0x100,149999)&&f.due(0x100,150000));
 });
 runTest("capacity_duplicate_and_malformed_does_not_mutate",[]{BrakeCanFilter f;uint8_t deny[]{0};f.apply(deny,1);
  for(uint8_t i=1;i<=6;++i){uint8_t c[]{2,0,0,0,0,0,i};CHECK(f.apply(c,7));CHECK(f.apply(c,7));}
  uint8_t overflow[]{2,0,0,0,0,0,7};CHECK(!f.apply(overflow,7)&&!f.due(7,0));CHECK(f.due(6,0));
  for(auto bad:std::vector<std::vector<uint8_t>>{{},{0,1},{1},{1,0},{2,0,0},{7,0,0}})CHECK(!f.apply(bad.data(),bad.size()));CHECK(f.due(1,0));
 });
}
