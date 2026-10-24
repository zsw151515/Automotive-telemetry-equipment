#include "TelemetryUi.h"
#include "RasterPrimitives.h"
#include <algorithm>
#include <cstdio>
namespace wired_brake {
void TelemetryUi::commandResult(const BrakeAck&a){
 if(a.result==AckResult::Pending){if(pending_&&!tracked_.commandId){tracked_=a;result_=a.result;}return;}
 if(!tracked_.commandId||a.hostBootId!=tracked_.hostBootId||a.commandId!=tracked_.commandId||a.targetBrakeBootId!=tracked_.targetBrakeBootId||a.op!=tracked_.op)return;
 if(a.result==AckResult::AppliedSaved){const auto&s=latest_.state;if(!s.saved||s.brakeBootId!=a.targetBrakeBootId||s.configVersion!=a.configVersion||!(s.config==draft_))return;}
 if(a.result!=AckResult::None){result_=a.result;pending_=false;if(a.result==AckResult::AppliedSaved)editing_=false;}
}
void TelemetryUi::ingest(const BrakeDelivery&d,uint64_t now){
 const auto view=updateBrakeView(d,now);if(!view.stateFresh)return;
 const bool newBoot=!have_||d.state.brakeBootId!=latest_.state.brakeBootId;
 if(newBoot){draft_=d.state.config;editing_=pending_=false;tracked_={};result_=AckResult::None;cancel();}
 else if(!editing_&&!pending_)draft_=d.state.config;
 latest_=d;have_=true;
 commandResult(d.state.ack);
 if(result_==AckResult::OutcomeUnknown&&d.state.saved&&d.state.config==draft_&&d.state.ack.result!=AckResult::OutcomeUnknown){result_=AckResult::AppliedSaved;editing_=false;}
}
int TelemetryUi::hit(uint16_t x,uint16_t y)const{
 if(y<48){if(x<250)return 0;if(x<500)return 1;if(x<750)return 2;}
 if(page_==Page::Settings){if(y>=280&&y<370){if(x>=80&&x<230)return 3;if(x>=270&&x<420)return 4;if(x>=480&&x<780)return 5;}if(x>=780&&x<1006&&y>=390&&y<470)return 6;}
 if(page_!=Page::Tire&&x>=650&&x<1006&&y>=480&&y<560)return 7;
 return -1;
}
UiAction TelemetryUi::activate(int target){
 if(target>=0&&target<=2){page_=Page(target);return {UiActionKind::Navigate,{}};}
 if(target==3||target==4){const int value=int(draft_.alarmC)+(target==3?-10:10);draft_.alarmC=uint16_t(std::clamp(value,100,900));editing_=true;result_=AckResult::None;return {UiActionKind::DraftChanged,{}};}
 if(target==5){draft_.warningBeep=!draft_.warningBeep;editing_=true;result_=AckResult::None;return {UiActionKind::DraftChanged,{}};}
 if(target==6||target==7){pending_=true;tracked_={};result_=AckResult::Pending;return {target==6?UiActionKind::Apply:UiActionKind::Mute,{target==6?CommandOp::ApplySave:CommandOp::Mute60,draft_}};}
 return {};
}
UiAction TelemetryUi::touch(const TouchSample&s,uint64_t now){
 if((clockSet_&&now<lastClock_)||!s.valid||s.sampledUs>now||now-s.sampledUs>200000||s.count>1||s.x>=1024||s.y>=600||(s.pressed&&s.count!=1)||(!s.pressed&&s.count!=0)){lastClock_=now;clockSet_=true;cancel();return {};}
 clockSet_=true;lastClock_=now;
 if(!s.pressed){pressed_=blocked_=fired_=false;target_=-1;return {};}
 if(blocked_)return {};
 const int target=hit(s.x,s.y);
 if(!pressed_){pressed_=true;pressedAt_=now;target_=target;fired_=false;nextRepeat_=deadlineAfter(now,500000);}
 if(target!=target_){cancel();return {};}
 if(target<0)return {};
 if(target>2&&(pending_||!fresh(now)||latest_.state.ack.result==AckResult::OutcomeUnknown))return {};
 const uint64_t hold=target==7?1000000:50000;
 if(!fired_&&now-pressedAt_>=hold){fired_=true;return activate(target);}
 if(fired_&&(target==3||target==4)&&now>=nextRepeat_){nextRepeat_=deadlineAfter(now,200000);return activate(target);}
 return {};
}
void TelemetryUi::render(const std::array<wired_tire::SnapshotDelivery,4>&tires,uint64_t now,const wired_tire::ColorScale&scale,wired_tire::PixelSurface&s,bool simulation){
 using wired_tire::raster::fill;using wired_tire::raster::text;
 if(!wired_tire::raster::validSurface(s)||s.width!=1024||s.height!=600)return;
 constexpr uint16_t bg=0x0862,white=0xffff,muted=0x9cf3,cyan=0x07ff,red=0xf800,amber=0xfd20,green=0x07e0;
 fill(s,0,0,1024,600,bg);
 const char*pages[]{"TIRE","BRAKE","SETTINGS"};for(unsigned i=0;i<3;++i){fill(s,i*250,0,246,48,unsigned(page_)==i?0x2149:0x10a4);text(s,12+i*250,12,pages[i],3,white);}
 const auto view=updateBrakeView(latest_,now);const auto&b=latest_.state;const char*state=!view.stateFresh?"BRAKE CONTROLLER LOST":b.state==BrakeState::Alarm?"BRAKE ALARM":b.state==BrakeState::Warn?"BRAKE WARNING":b.state==BrakeState::Fail?"BRAKE SENSOR FAIL":"BRAKE OK";
 const uint16_t alarm=!view.stateFresh?amber:b.state==BrakeState::Alarm?red:b.state==BrakeState::Warn||b.state==BrakeState::Fail?amber:green;
 fill(s,0,52,1024,40,alarm);text(s,12,60,state,3,bg);if(view.stateFresh&&b.validMask!=b.enabledMask)text(s,700,65,"PROBE LOST",2,bg);
 char label[80]{};
 if(page_==Page::Tire){const char*names[]{"FL","FR","RL","RR"};for(unsigned w=0;w<4;++w){const auto&d=tires[w];const auto v=wired_tire::updateView(d,now);const size_t left=(w%2)*512,top=(w/2)*235;
   text(s,left+12,top+102,names[w],3,white);const size_t offset=(top+130)*s.stride+left+12;wired_tire::PixelSurface tile{s.pixels+offset,256,192,s.stride,s.capacity-offset};wired_tire::renderHeatmap(d.snapshot.frame,scale,tile,!v.fresh,d.snapshot.zones.mirrored);
   text(s,left+286,top+140,v.fresh?"LIVE":"NO FRESH DATA",2,v.fresh?green:amber);int16_t maximum=0;const bool ok=wired_tire::maximumTemperature(d.snapshot.frame,maximum);
   if(ok&&v.fresh)std::snprintf(label,sizeof(label),"MAX %.1f C",maximum/10.);else std::snprintf(label,sizeof(label),"MAX -- C");text(s,left+286,top+182,label,3,white);
   std::snprintf(label,sizeof(label),"AGE %luMS",static_cast<unsigned long>(v.ageMs));text(s,left+286,top+230,label,2,muted);text(s,left+286,top+268,d.snapshot.meta.bleConnected?"BLE ON":"BLE WAIT",2,muted);
  }}
 else if(page_==Page::Brake){for(unsigned i=0;i<2;++i){const size_t x=i*512+12;fill(s,x,108,490,168,0x10c5);text(s,x+12,120,i?"FR DISC MEASURED":"FL DISC MEASURED",3,muted);
   if(view.sensorFresh[i]&&view.stateFresh)std::snprintf(label,sizeof(label),"%.1f C",b.temp10[i]/10.);else std::snprintf(label,sizeof(label),"--.- C");text(s,x+12,175,label,7,white);}
  if(view.stateFresh&&b.modelValid)std::snprintf(label,sizeof(label),"CORE EST %.1f C    FAST EST %.1f C",b.core10/10.,b.fast10/10.);else std::snprintf(label,sizeof(label),"CORE EST -- C    FAST EST -- C");text(s,24,315,label,3,cyan);
  if(view.stateFresh)std::snprintf(label,sizeof(label),"DOSE %.1f    ALM %u C    BLE %s",b.dose10/10.,b.config.alarmC,b.bleConnected?"ON":"WAIT");else std::snprintf(label,sizeof(label),"DOSE --    ALM -- C    BLE UNKNOWN");text(s,24,370,label,3,muted);
  text(s,24,430,"CORE / FAST / DOSE ARE ESTIMATES",2,amber);
 }
 else{
  if(view.stateFresh)std::snprintf(label,sizeof(label),"EFFECTIVE ALM %u C  WARN BEEP %s",b.config.alarmC,b.config.warningBeep?"ON":"OFF");else std::snprintf(label,sizeof(label),"EFFECTIVE CONFIG UNKNOWN");text(s,24,115,label,3,white);
  std::snprintf(label,sizeof(label),"DRAFT ALM %u C",draft_.alarmC);text(s,24,185,label,5,cyan);
  if(view.stateFresh&&draft_.alarmC!=b.config.alarmC)text(s,24,244,"NEW ALM RESETS MODEL AND ALARM",2,amber);
  fill(s,80,280,150,90,0x2149);text(s,130,305,"-",6,white);fill(s,270,280,150,90,0x2149);text(s,320,305,"+",6,white);
  fill(s,480,280,300,90,0x2149);text(s,500,310,draft_.warningBeep?"WARN BEEP ON":"WARN BEEP OFF",3,white);
  fill(s,780,390,226,80,pending_?0x4208:0x036c);text(s,795,415,pending_?"WAIT":"APPLY SAVE",3,white);
  const char*status=pending_?"PENDING CONFIRMATION":result_==AckResult::OutcomeUnknown?"RESULT UNKNOWN - READ ACTUAL":result_==AckResult::AppliedSaved?"ACTUAL CONFIG SAVED":result_==AckResult::StorageFailed?"SAVE FAILED - CHECK ACTUAL":editing_?"DRAFT ONLY - NOT APPLIED":b.recovered?"RECOVERED - REVIEW CONFIG":b.saved?"ACTUAL CONFIG SAVED":"DEFAULT - NOT SAVED";
  text(s,24,410,status,2,amber);text(s,24,450,"ALARM ALWAYS OVERRIDES WARN BEEP OFF",2,muted);
 }
 if(page_!=Page::Tire){fill(s,650,480,356,80,0x2149);text(s,668,495,"HOLD 1S MUTE 60S",3,white);std::snprintf(label,sizeof(label),"MUTE %luS",static_cast<unsigned long>((b.muteRemainingMs+999)/1000));text(s,668,535,label,2,muted);}
 std::snprintf(label,sizeof(label),"SCALE %.0f..%.0f C  /  32X24 ALL PIXELS",scale.minimum10/10.,scale.maximum10/10.);text(s,12,580,label,2,muted);
 if(simulation)text(s,780,580,"SIMULATED DATA",2,amber);
}
}
