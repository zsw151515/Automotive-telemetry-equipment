#include "brake_test_support.h"
#include "BrakeWire.h"
int main(){
 runTest("wire_offsets_signed_crc_and_transactional_reject",[]{auto s=validBrakeTelemetry();s.temp10[0]=-123;s.temp10[1]=6100;std::array<uint8_t,256>b{};
  CHECK(encodeBrakeStatus(s,0x12345678,b.data(),256));CHECK(b[16]==0x78&&b[17]==0x56&&b[44]==0x85&&b[45]==0xff&&b[6]==1);
  auto repaired=b;repairBrakeCrc(repaired);CHECK(b==repaired);BrakeTelemetry out{};CHECK(decodeBrakeStatus(b.data(),256,0x12345678,out)&&out.temp10[0]==-123);
  out.sequence=99;CHECK(!decodeBrakeStatus(b.data(),256,1,out)&&out.sequence==99);
  for(size_t n=0;n<256;++n)CHECK(!decodeBrakeStatus(b.data(),n,0x12345678,out)&&out.sequence==99);
  for(size_t offset:{size_t(6),size_t(7),size_t(42),size_t(43),size_t(84),size_t(85),size_t(86),size_t(104),size_t(105),size_t(106),size_t(247),size_t(252)}){
   auto bad=b;bad[offset]=255;repairBrakeCrc(bad);CHECK(!decodeBrakeStatus(bad.data(),256,0x12345678,out)&&out.sequence==99);}
 });
 runTest("request_read_canonical_and_mutation_validation",[]{std::array<uint8_t,256>b{};auto c=saveCommand();uint32_t token=99;BrakeCommand out{};
  CHECK(encodeBrakeRequest(c,7,b.data(),256));CHECK(b[28]==0x8a&&b[29]==2);CHECK(decodeBrakeRequest(b.data(),256,out,token)&&out.candidate.alarmC==650&&token==7);
  for(size_t i:{size_t(6),size_t(31),size_t(36),size_t(247),size_t(252)}){auto bad=b;bad[i]=1;repairBrakeCrc(bad);out.commandId=99;CHECK(!decodeBrakeRequest(bad.data(),256,out,token)&&out.commandId==99);}
  CHECK(!encodeBrakeRequest(c,0,b.data(),256));c.targetBrakeBootId=0;CHECK(!encodeBrakeRequest(c,1,b.data(),256));
  c=readCommand();CHECK(encodeBrakeRequest(c,1,b.data(),256));for(size_t i=20;i<36;++i)CHECK(b[i]==0);
  b[28]=1;repairBrakeCrc(b);CHECK(!decodeBrakeRequest(b.data(),256,out,token));
 });
}
