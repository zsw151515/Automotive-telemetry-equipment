#include "BrakeLinkMaster.h"
#include <esp_system.h>
using namespace wired_brake;
void BrakeLinkMaster::unknown(){ack_.result=AckResult::OutcomeUnknown;mutating_=false;readingBack_=true;next_=now_;if(wireCommand_.op!=CommandOp::Read)window_=false;}
SubmitResult BrakeLinkMaster::submit(const ControlIntent&intent){
 if(mutating_)return SubmitResult::Busy;
 if(readingBack_||!known_||!updateBrakeView(latest_,now_).stateFresh||!latest_.state.configValid)return SubmitResult::NotReady;
 if((intent.op!=CommandOp::ApplySave&&intent.op!=CommandOp::Mute60)||(intent.op==CommandOp::ApplySave&&!validConfig(intent.candidate))||id_==UINT32_MAX)return SubmitResult::Invalid;
 command_={};command_.op=intent.op;command_.hostBootId=host_;command_.targetBrakeBootId=latest_.state.brakeBootId;command_.commandId=++id_;command_.expectedConfigVersion=latest_.state.configVersion;command_.candidate=intent.candidate;if(intent.op==CommandOp::Mute60)command_.muteMs=60000;
 ack_={AckResult::Pending,intent.op,host_,id_,command_.targetBrakeBootId,command_.expectedConfigVersion};mutating_=true;mutationStart_=now_;next_=now_;return SubmitResult::Accepted;
}
bool BrakeLinkMaster::poll(uint64_t now,BrakeDelivery&out){
 if(!bus_.enabled())return false;
 const bool backwards=now<now_;now_=now;bus_.poll(now);
 if(!host_){host_=esp_random();if(!host_)host_=1;token_=esp_random();}
 if(mutating_&&(backwards||now<mutationStart_||now-mutationStart_>=1000000))unknown();
 BusResult result{};bool delivered=false;
 if(bus_.takeCompleted(BoardPeer::Brake,result)){
  inFlight_=false;BrakeTelemetry s{};
  if(window_&&result.success&&now>=started_&&now-started_<100000&&decodeBrakeStatus(rx_.data(),256,token_,s)){
   out={s,now,elapsedMs(now,started_)};latest_=out;delivered=true;window_=false;next_=deadlineAfter(now,250000);known_=true;
   const bool fresh=updateBrakeView(out,now).stateFresh;
   if(mutating_){const auto&a=s.ack;const bool matching=s.brakeBootId==command_.targetBrakeBootId&&a.hostBootId==host_&&a.commandId==command_.commandId&&a.targetBrakeBootId==command_.targetBrakeBootId&&a.op==command_.op;
    if(matching&&a.result!=AckResult::None&&a.result!=AckResult::Pending){
     ack_=a;mutating_=false;
     if((a.result==AckResult::Muted&&!fresh)||(a.result==AckResult::AppliedSaved&&(!fresh||!s.saved||s.configVersion!=a.configVersion||!(s.config==command_.candidate))))unknown();
     else if(a.result==AckResult::OutcomeUnknown)readingBack_=true;
    }else if(s.brakeBootId!=command_.targetBrakeBootId||s.configVersion!=command_.expectedConfigVersion)unknown();
   }
   if(readingBack_&&wireCommand_.op==CommandOp::Read&&fresh&&s.ack.result!=AckResult::OutcomeUnknown&&s.ack.result!=AckResult::Pending)readingBack_=false;
  }
 }
 if(window_&&(now<started_||now-started_>=100000)){window_=false;next_=deadlineAfter(now,500000);}
 if(!window_&&!inFlight_&&now>=next_){
  wireCommand_=mutating_?command_:BrakeCommand{};
  if(!mutating_){wireCommand_.hostBootId=host_;wireCommand_.targetBrakeBootId=0;}
  ++token_;if(!token_)++token_;started_=now;window_=encodeBrakeRequest(wireCommand_,token_,tx_.data(),256);
 }
 if(window_&&!inFlight_)inFlight_=bus_.tryQueue(BoardPeer::Brake,tx_.data(),rx_.data(),256);
 return delivered;
}
