#include "test_support.h"
#include "Board7BRules.h"
#include "Gt911TouchCodec.h"
using namespace wired_brake;
static Board7BPinPlan good(){return {{2,3,4},{5,28,29,30,31},{49,50,51,52,47},true,true,true};}
int main(){
 runTest("board_profile_individual_failure_reasons",[]{auto p=good();CHECK(validateBoardPins(p)==ProfileResult::Ok);p.electricalApproved=false;CHECK(validateBoardPins(p)==ProfileResult::Unapproved);p=good();p.bus[0]=-1;CHECK(validateBoardPins(p)==ProfileResult::Unassigned);p=good();p.bus[0]=p.cs[0];CHECK(validateBoardPins(p)==ProfileResult::Duplicate);
  for(int pin:{32,33,26,7,8,23,6,35}){p=good();p.bus[0]=pin;CHECK(validateBoardPins(p)==ProfileResult::Reserved);}
  p=good();p.cs[0]=34;p.strapApproved=false;CHECK(validateBoardPins(p)==ProfileResult::NeedsStrapReview);p=good();p.vo4Approved=false;CHECK(validateBoardPins(p)==ProfileResult::NeedsVo4Review);
 });
 runTest("gt911_zero_edges_multitouch_corrupt_and_transactional_decode",[]{TouchSample s{};uint8_t b[8]{};CHECK(decodeGt911Report(0x81,b,8,s)&&s.pressed&&s.x==0&&s.y==0);b[1]=255;b[2]=3;b[3]=87;b[4]=2;CHECK(decodeGt911Report(0x81,b,8,s)&&s.x==1023&&s.y==599);
  b[1]=0;b[2]=4;CHECK(!decodeGt911Report(0x81,b,8,s)&&s.x==1023);CHECK(!decodeGt911Report(0x82,b,8,s));CHECK(!decodeGt911Report(0x91,b,8,s));CHECK(!decodeGt911Report(1,b,8,s));CHECK(decodeGt911Report(0x80,nullptr,0,s)&&!s.pressed&&s.count==0);
 });
}
