#pragma once
#include "BrakeTypes.h"
#include "ProbeSchedule.h"
#include <cstddef>
namespace wired_brake {
bool decodeBrakeRegister(const uint8_t*,size_t,uint8_t address,uint8_t function,int16_t& temp10);
bool encodeBrakeRead(uint8_t address,uint8_t function,uint16_t reg,uint8_t out[8]);
enum class PollAction {None,Request,Timeout};
class BrakePoller {
 public:
  PollAction poll(uint64_t nowUs);
  bool response(uint64_t nowUs,bool accepted);
  uint8_t function()const{return (candidate_&1)?4:3;}
  uint16_t reg()const{return candidate_/2;}
  bool pending()const{return schedule_.pending();}
  bool hasGood()const{return schedule_.hasGoodResponse();}
  uint64_t lastGoodUs()const{return schedule_.lastGoodUs();}
  bool fresh(uint64_t now)const{return hasGood()&&now>=lastGoodUs()&&now-lastGoodUs()<=2500000;}
 private:
  wired_tire::ProbeSchedule schedule_{20000,200000,100000};uint8_t candidate_=0;bool advance_=false;
};
}
