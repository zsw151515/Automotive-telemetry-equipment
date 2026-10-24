#pragma once
#include "BrakeTypes.h"
namespace wired_brake {
class BrakeBuzzer {
 public:
  void begin(uint64_t bootUs){boot_=bootUs;muted_=false;begun_=true;lastState_=BrakeState::Fail;}
  void mute60(uint64_t now){muted_=true;muteStart_=now;}
  uint32_t remainingMs(uint64_t now)const;
  bool output(BrakeState,bool warningEnabled,uint64_t now);
 private:
  uint64_t boot_=0,muteStart_=0;bool muted_=false,begun_=false;BrakeState lastState_=BrakeState::Fail;
};
}
