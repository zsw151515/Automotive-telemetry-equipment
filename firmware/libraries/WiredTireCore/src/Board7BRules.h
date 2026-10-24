#pragma once
#include <array>
namespace wired_brake {
struct Board7BPinPlan{std::array<int,3>bus{{-1,-1,-1}};std::array<int,5>cs{{-1,-1,-1,-1,-1}},ready{{-1,-1,-1,-1,-1}};bool electricalApproved=false,strapApproved=false,vo4Approved=false;};
enum class ProfileResult{Ok,Unapproved,Unassigned,Duplicate,Reserved,NeedsStrapReview,NeedsVo4Review};
ProfileResult validateBoardPins(const Board7BPinPlan&);
}
