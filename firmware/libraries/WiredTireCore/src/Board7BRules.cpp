#include "Board7BRules.h"
namespace wired_brake {
ProfileResult validateBoardPins(const Board7BPinPlan&p){
 if(!p.electricalApproved)return ProfileResult::Unapproved;
 std::array<int,13>pins{};for(unsigned i=0;i<3;++i)pins[i]=p.bus[i];for(unsigned i=0;i<5;++i){pins[3+i]=p.cs[i];pins[8+i]=p.ready[i];}
 for(unsigned i=0;i<13;++i){const int pin=pins[i];if(pin<0)return ProfileResult::Unassigned;for(unsigned j=0;j<i;++j)if(pin==pins[j])return ProfileResult::Duplicate;
  bool exposed=false;for(int v:{36,34,31,30,29,28,5,4,3,2,52,51,50,49,48,47,46})if(v==pin)exposed=true;
  if(!exposed)return ProfileResult::Reserved;
  if((pin==34||pin==36)&&!p.strapApproved)return ProfileResult::NeedsStrapReview;
  if(pin>=46&&pin<=48&&!p.vo4Approved)return ProfileResult::NeedsVo4Review;
 }
 return ProfileResult::Ok;
}
}
