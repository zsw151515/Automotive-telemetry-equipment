#include "BrakeCan.h"
#include <algorithm>
namespace wired_brake {
size_t encodeBrakeCan(uint32_t pid,const BrakeTelemetry&s,uint8_t*b){
 if(!b||pid<0x100||pid>0x107)return 0;
 for(unsigned i=0;i<4;++i)b[i]=uint8_t(pid>>(i*8));
 if(pid==0x106){b[4]=uint8_t(s.state);return 5;}
 int16_t temperature=kInvalidTemperature;uint16_t word=0;
 if(pid==0x100||pid==0x101){const unsigned i=pid-0x100;if(s.validMask&(1<<i))temperature=s.temp10[i];}
 if(pid==0x102&&s.validMask)temperature=s.max10;
 if(pid==0x103&&s.modelValid)temperature=s.fast10;
 if(pid==0x104&&s.modelValid)temperature=s.core10;
 if(pid<=0x104){if(temperature< -400||temperature>12000)temperature=kInvalidTemperature;word=uint16_t(temperature);}
 else word=pid==0x105?s.dose10:s.config.alarmC;
 b[4]=uint8_t(word>>8);b[5]=uint8_t(word);return 6;
}
bool BrakeCanFilter::apply(const uint8_t*b,size_t n){
 if(!b||!n)return false;
 if(b[0]==0&&n==1){allowAll_=false;entries_={};return true;}
 if(n<3)return false;
 const uint16_t interval=std::max(uint16_t(150),uint16_t(uint16_t(b[1])<<8|b[2]));
 if(b[0]==1){allowAll_=true;allInterval_=interval;allTimers_={};entries_={};return true;}
 if(b[0]!=2||n<7)return false;
 uint32_t be=0,le=0;for(unsigned i=0;i<4;++i){be=be<<8|b[3+i];le|=uint32_t(b[3+i])<<(8*i);}
 auto next=entries_;
 for(uint32_t pid:{be,le}){
  Entry* target=nullptr;for(auto&e:next)if(e.used&&e.pid==pid){target=&e;break;}
  if(!target)for(auto&e:next)if(!e.used){target=&e;break;}
  if(!target)return false;
  if(!target->used)*target={pid,interval,0,true,false};else target->intervalMs=interval;
 }
 entries_=next;allowAll_=false;return true;
}
bool BrakeCanFilter::due(uint32_t pid,uint64_t now){
 Entry* e=nullptr;
 if(allowAll_){if(pid<0x100||pid>0x107)return false;e=&allTimers_[pid-0x100];e->intervalMs=allInterval_;}
 else for(auto& entry:entries_)if(entry.used&&entry.pid==pid){e=&entry;break;}
 if(!e)return false;
 if(e->sent&&now>=e->last&&now-e->last<uint64_t(e->intervalMs)*1000)return false;
 e->last=now;e->sent=true;return true;
}
}
