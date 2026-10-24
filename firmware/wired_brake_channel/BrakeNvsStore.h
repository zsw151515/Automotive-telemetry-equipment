#pragma once
#include "BrakeChannelConfig.h"
class BrakeNvsStore:public wired_brake::ConfigStore {
 public:
  WT_OFFLINE_LINK bool begin(const BrakeHardwareConfig&c){enabled_=validBrakeHardware(c);return enabled_;}
  WT_OFFLINE_LINK wired_brake::LoadResult load(wired_brake::PersistRecord&)override;
  WT_OFFLINE_LINK wired_brake::StoreResult commit(const wired_brake::PersistRecord&)override;
  WT_OFFLINE_LINK wired_brake::LoadResult readBack(uint32_t,wired_brake::PersistRecord&)override;
  uint64_t maxPauseUs()const{return maxPause_;}
 private:bool enabled_=false;uint64_t maxPause_=0;
  wired_brake::LoadResult loadSlots(wired_brake::PersistRecord&);
};
