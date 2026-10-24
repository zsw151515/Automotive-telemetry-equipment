#include "test_support.h"
#include "BrakeModbus.h"
using namespace wired_brake;
int main() {
  runTest("zero_negative_and_limits", [] {
    for (int x : {0,-400,12000,-401,12001}) {
      std::vector<uint8_t> f{1,3,2,uint8_t(x>>8),uint8_t(x),0,0}; repairFixtureCrc(f);
      int16_t out=999; bool ok=decodeBrakeRegister(f.data(),f.size(),1,3,out);
      CHECK(ok==(x>=-400&&x<=12000)); CHECK(out==(ok?x:999));
    }
  });
  runTest("bad_address_function_crc_exception_truncation_preserves_output", [] {
    std::vector<uint8_t> f{1,4,2,0,0,0,0}; repairFixtureCrc(f); int16_t out=123;
    CHECK(!decodeBrakeRegister(f.data(),f.size(),2,4,out));
    CHECK(!decodeBrakeRegister(f.data(),f.size(),1,3,out));
    for(size_t n=0;n<f.size();++n) CHECK(!decodeBrakeRegister(f.data(),n,1,4,out));
    f[2]=1;repairFixtureCrc(f);CHECK(!decodeBrakeRegister(f.data(),7,1,4,out));
    f={1,0x84,2,0,0};repairFixtureCrc(f);CHECK(!decodeBrakeRegister(f.data(),5,1,4,out));
    f[4]^=1;CHECK(!decodeBrakeRegister(f.data(),5,1,4,out));CHECK(out==123);
  });
  runTest("late_tail_never_renews_age", [] {
    BrakePoller a,b; CHECK(!a.response(0,true)); CHECK(a.poll(0)==PollAction::Request);
    CHECK(a.function()==3&&a.reg()==0); CHECK(!a.response(20000,true));CHECK(!a.hasGood());
    CHECK(b.poll(100000)==PollAction::Request); CHECK(b.response(119999,true)); CHECK(b.lastGoodUs()==119999);
    for(unsigned i=1;i<4;++i) { CHECK(a.poll(i*200000)==PollAction::Request);
      CHECK(a.function()==((i&1)?4:3)&&a.reg()==i/2); CHECK(!a.response(i*200000+1000,false)); }
    CHECK(b.fresh(2619999));CHECK(!b.fresh(2620000));
  });
}
