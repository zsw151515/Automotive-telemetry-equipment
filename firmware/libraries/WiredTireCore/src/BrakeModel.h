#pragma once
#include "BrakeTypes.h"
namespace wired_brake {
class BrakeModel {
 public:
  BrakeModelResult tick(const std::array<BrakeReading,2>&,uint8_t,const BrakeConfig&,uint64_t);
  void reset(uint64_t nowUs);
  const BrakeModelResult& result()const{return result_;}
 private:
  BrakeModelResult result_{};double fast_=0,core_=0,dose_=0;uint64_t last_=0,coreStart_=0,nearStart_=0;
  bool initialized_=false,coreHold_=false,nearHold_=false,alarm_=false;
};
}
