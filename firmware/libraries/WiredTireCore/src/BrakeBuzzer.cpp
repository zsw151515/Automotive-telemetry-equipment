#include "BrakeBuzzer.h"
namespace wired_brake {
uint32_t BrakeBuzzer::remainingMs(uint64_t now)const{
  if(!muted_||now<muteStart_||now-muteStart_>=60000000)return 0;
  const auto d=60000000-(now-muteStart_);return uint32_t(d/1000+(d%1000!=0));
}
bool BrakeBuzzer::output(BrakeState state,bool warning,uint64_t now){
  if(lastState_==BrakeState::Alarm&&state!=BrakeState::Alarm)muted_=false;
  lastState_=state;
  if(!begun_||now<boot_||remainingMs(now))return false;
  const auto startup=now-boot_;
  if(startup<180000)return warning&&(startup<60000||startup>=140000);
  if(startup<3000000)return false;
  if(state==BrakeState::Alarm)return (now-boot_)%300000<150000;
  if(state==BrakeState::Warn&&warning)return (now-boot_)%1500000<80000;
  return false;
}
}
