#include "BrakeModbus.h"
#include "Yf90640Protocol.h"
namespace wired_brake {
bool decodeBrakeRegister(const uint8_t* b,size_t n,uint8_t address,uint8_t function,int16_t& out){
  if(!b||n!=7||address<1||address>247||(function!=3&&function!=4)||b[0]!=address||b[1]!=function||b[2]!=2)return false;
  const auto crc=wired_tire::modbusCrc(b,n-2);
  if(b[n-2]!=uint8_t(crc)||b[n-1]!=uint8_t(crc>>8))return false;
  const int16_t value=static_cast<int16_t>((uint16_t(b[3])<<8)|b[4]);
  if(value< -400||value>12000)return false;
  out=value;return true;
}
bool encodeBrakeRead(uint8_t a,uint8_t f,uint16_t r,uint8_t out[8]){
  if(!out||a<1||a>247||(f!=3&&f!=4)||r>1)return false;
  out[0]=a;out[1]=f;out[2]=0;out[3]=uint8_t(r);out[4]=0;out[5]=1;
  const auto crc=wired_tire::modbusCrc(out,6);out[6]=uint8_t(crc);out[7]=uint8_t(crc>>8);return true;
}
PollAction BrakePoller::poll(uint64_t now){
  const auto action=schedule_.poll(now);
  if(action==wired_tire::ProbeAction::Timeout){advance_=true;return PollAction::Timeout;}
  if(action==wired_tire::ProbeAction::Request){if(advance_){candidate_=(candidate_+1)%4;advance_=false;}return PollAction::Request;}
  return PollAction::None;
}
bool BrakePoller::response(uint64_t now,bool accepted){
  const bool wasPending=schedule_.pending();const bool ok=schedule_.response(now,accepted);
  if(wasPending&&!ok)advance_=true;
  return ok;
}
}
