#include "BrakeModel.h"
#include <algorithm>
#include <cmath>
namespace wired_brake {
void BrakeModel::reset(uint64_t now){result_={};fast_=core_=dose_=0;last_=now;initialized_=coreHold_=nearHold_=alarm_=false;}
BrakeModelResult BrakeModel::tick(const std::array<BrakeReading,2>& r,uint8_t enabled,const BrakeConfig& cfg,uint64_t now){
  uint8_t valid=0;int16_t maximum=-400;
  for(unsigned i=0;i<2;++i)if((enabled&(1<<i))&&r[i].valid&&r[i].temp10>=-400&&r[i].temp10<=12000&&now>=r[i].receivedUs&&now-r[i].receivedUs<=2500000){valid|=1<<i;maximum=std::max(maximum,r[i].temp10);}
  if(!valid||!validConfig(cfg)){reset(now);result_.failMask=enabled&3;return result_;}
  const bool restart=!initialized_||now<last_||now-last_>2000000;
  double dt=0;
  if(restart){reset(now);initialized_=true;fast_=core_=maximum/10.0;}
  else{dt=double(now-last_)/1e6;const double input=maximum/10.0;fast_+=(input-fast_)*dt/(.8+dt);core_+=(input-core_)*dt/((input>core_?20.:45.)+dt);}
  last_=now;
  const double start=std::max(double(cfg.alarmC)-80.,80.);
  if(core_>=start)dose_+=std::clamp((core_-start)/80.,0.,2.5)*dt;
  else dose_-=.7*dt;
  dose_=std::clamp(dose_,0.,99.);
  if(!std::isfinite(core_)||!std::isfinite(fast_)||!std::isfinite(dose_)){reset(now);return result_;}
  const auto held=[now](bool condition,bool& holding,uint64_t& began,uint64_t duration){
    if(!condition){holding=false;return false;}if(!holding){holding=true;began=now;}return now>=began&&now-began>=duration;
  };
  const bool c=held(core_>=cfg.alarmC,coreHold_,coreStart_,6000000);
  const bool n=held(core_>=cfg.alarmC-20.&&(dose_>=20.||fast_>=cfg.alarmC+40.),nearHold_,nearStart_,5000000);
  if(c||n)alarm_=true;
  if(alarm_&&core_<=cfg.alarmC-40.&&dose_<=8.&&fast_<cfg.alarmC)alarm_=false;
  const bool warn=core_>=cfg.alarmC-30.||dose_>=10.||fast_>=cfg.alarmC;
  result_.max10=maximum;result_.fast10=static_cast<int16_t>(std::lround(fast_*10));result_.core10=static_cast<int16_t>(std::lround(core_*10));
  result_.dose10=static_cast<uint16_t>(std::lround(dose_*10));result_.anyFresh=result_.modelValid=true;result_.failMask=(enabled&3)&~valid;
  result_.state=alarm_?BrakeState::Alarm:warn?BrakeState::Warn:result_.failMask?BrakeState::Fail:BrakeState::Ok;return result_;
}
}
