#pragma once
#include "BoardSpiBus.h"
enum class SubmitResult{Accepted,Busy,NotReady,Invalid};
class BrakeLinkMaster {
 public:
  explicit BrakeLinkMaster(BoardSpiBus&bus):bus_(bus){}
  WT_OFFLINE_LINK bool poll(uint64_t now,wired_brake::BrakeDelivery&);
  SubmitResult submit(const wired_brake::ControlIntent&);
  wired_brake::BrakeAck pendingResult()const{return ack_;}
 private:
  BoardSpiBus&bus_;wired_brake::BrakeDelivery latest_{};wired_brake::BrakeCommand command_{},wireCommand_{};wired_brake::BrakeAck ack_{};
  std::array<uint8_t,256>tx_{},rx_{};uint32_t host_=0,id_=0,token_=0;uint64_t now_=0,next_=0,started_=0,mutationStart_=0;
  bool known_=false,mutating_=false,readingBack_=false,window_=false,inFlight_=false;
  void unknown();
};
