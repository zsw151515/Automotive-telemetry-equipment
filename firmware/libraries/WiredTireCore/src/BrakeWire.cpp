#include "BrakeWire.h"
#include "SnapshotWire.h"
#include <algorithm>
#include <cstring>
namespace wired_brake {
namespace {
void put(uint8_t*p,uint64_t v,unsigned n=4){for(unsigned i=0;i<n;++i)p[i]=uint8_t(v>>(i*8));}
uint64_t get(const uint8_t*p,unsigned n=4){uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(p[i])<<(i*8);return v;}
void crc(uint8_t*b){put(b+248,wired_tire::wireCrc32(b,248));}
bool base(const uint8_t*b,size_t n,const char* magic){return b&&n==256&&!std::memcmp(b,magic,4)&&b[4]==1&&get(b+248)==wired_tire::wireCrc32(b,248);}
bool temp(int16_t t){return t==kInvalidTemperature||(t>=-400&&t<=12000);}
bool valid(const BrakeTelemetry&s){
 if(!s.brakeBootId||uint8_t(s.state)>3||!validConfig(s.config)||(s.validMask&~s.enabledMask)||(s.enabledMask&~3)||s.configOrigin>2||uint8_t(s.ack.result)>11||uint8_t(s.ack.op)>2||s.dose10>990||s.muteRemainingMs>60000)return false;
 if((!s.modelValid&&s.state!=BrakeState::Fail)||(s.modelValid&&!s.validMask)||(s.state==BrakeState::Ok&&s.validMask!=s.enabledMask))return false;
 for(unsigned i=0;i<2;++i)if(!temp(s.temp10[i])||((s.validMask&(1<<i))&&s.temp10[i]==kInvalidTemperature))return false;
 if(!temp(s.max10)||!temp(s.fast10)||!temp(s.core10))return false;
 if((s.validMask==0)!=(s.max10==kInvalidTemperature))return false;
 if(s.modelValid?(s.fast10==kInvalidTemperature||s.core10==kInvalidTemperature):(s.fast10!=kInvalidTemperature||s.core10!=kInvalidTemperature))return false;
 if(s.saved&&(!s.configValid||s.savedVersion!=s.configVersion||!s.savedVersion))return false;
 return true;
}
uint32_t sumAge(uint32_t a,uint32_t b,uint32_t c){const uint64_t v=uint64_t(a)+b+c;return v>=UINT32_MAX?UINT32_MAX:uint32_t(v);}
}
bool encodeBrakeRequest(const BrakeCommand&c,uint32_t token,uint8_t*b,size_t n){
 if(!b||n!=256||!token||!c.hostBootId||uint8_t(c.op)>2)return false;
 if(c.op!=CommandOp::Read&&(!c.targetBrakeBootId||!c.commandId))return false;
 if(c.op==CommandOp::ApplySave&&!validConfig(c.candidate))return false;
 if(c.op==CommandOp::Mute60&&c.muteMs!=60000)return false;
 std::memset(b,0,n);std::memcpy(b,"BRC1",4);b[4]=1;b[5]=uint8_t(c.op);put(b+8,token);put(b+12,c.hostBootId);put(b+16,c.targetBrakeBootId);
 if(c.op!=CommandOp::Read){put(b+20,c.commandId);put(b+24,c.expectedConfigVersion);}
 if(c.op==CommandOp::ApplySave){put(b+28,c.candidate.alarmC,2);b[30]=c.candidate.warningBeep;}
 if(c.op==CommandOp::Mute60)put(b+32,c.muteMs);
 crc(b);return true;
}
bool decodeBrakeRequest(const uint8_t*b,size_t n,BrakeCommand&out,uint32_t&token){
 if(!base(b,n,"BRC1"))return false;
 BrakeCommand c{};c.op=CommandOp(b[5]);c.hostBootId=uint32_t(get(b+12));c.targetBrakeBootId=uint32_t(get(b+16));c.commandId=uint32_t(get(b+20));c.expectedConfigVersion=uint32_t(get(b+24));c.candidate={uint16_t(get(b+28,2)),b[30]!=0};c.muteMs=uint32_t(get(b+32));
 uint8_t canonical[256];const auto t=uint32_t(get(b+8));if(!encodeBrakeRequest(c,t,canonical,256)||std::memcmp(b,canonical,256))return false;
 out=c;token=t;return true;
}
bool encodeBrakeStatus(const BrakeTelemetry&s,uint32_t token,uint8_t*b,size_t n){
 if(!b||n!=256||!token||!valid(s))return false;
 std::memset(b,0,n);std::memcpy(b,"BRK1",4);b[4]=b[5]=1;b[6]=uint8_t(s.state);b[7]=s.bleEnabled|s.bleConnected<<1|s.configValid<<2|s.modelValid<<3|s.saved<<4|s.recovered<<5;
 put(b+8,s.brakeBootId);put(b+12,s.sequence);put(b+16,token);put(b+20,s.stateAgeMs);put(b+24,s.sourceStateUs,8);put(b+32,s.configVersion);put(b+36,s.savedVersion);put(b+40,s.config.alarmC,2);b[42]=s.config.warningBeep;b[43]=s.validMask;
 put(b+44,uint16_t(s.temp10[0]),2);put(b+46,uint16_t(s.temp10[1]),2);put(b+48,uint16_t(s.max10),2);put(b+50,uint16_t(s.fast10),2);put(b+52,uint16_t(s.core10),2);put(b+54,s.dose10,2);
 put(b+56,s.sensorAgeMs[0]);put(b+60,s.sensorAgeMs[1]);put(b+64,s.muteRemainingMs);put(b+68,s.crcErrors);put(b+72,s.lengthErrors);put(b+76,s.timeouts);put(b+80,s.recoveries);
 b[84]=uint8_t(s.ack.result);b[85]=uint8_t(s.ack.op);put(b+88,s.ack.hostBootId);put(b+92,s.ack.commandId);put(b+96,s.ack.targetBrakeBootId);put(b+100,s.ack.configVersion);b[104]=s.configOrigin;b[105]=s.enabledMask;crc(b);return true;
}
bool decodeBrakeStatus(const uint8_t*b,size_t n,uint32_t token,BrakeTelemetry&out){
 if(!base(b,n,"BRK1")||!token||get(b+16)!=token)return false;
 BrakeTelemetry s{};s.state=BrakeState(b[6]);s.bleEnabled=b[7]&1;s.bleConnected=b[7]&2;s.configValid=b[7]&4;s.modelValid=b[7]&8;s.saved=b[7]&16;s.recovered=b[7]&32;
 s.brakeBootId=uint32_t(get(b+8));s.sequence=uint32_t(get(b+12));s.stateAgeMs=uint32_t(get(b+20));s.sourceStateUs=get(b+24,8);s.configVersion=uint32_t(get(b+32));s.savedVersion=uint32_t(get(b+36));s.config={uint16_t(get(b+40,2)),b[42]!=0};s.validMask=b[43];
 s.temp10={int16_t(get(b+44,2)),int16_t(get(b+46,2))};s.max10=int16_t(get(b+48,2));s.fast10=int16_t(get(b+50,2));s.core10=int16_t(get(b+52,2));s.dose10=uint16_t(get(b+54,2));s.sensorAgeMs={uint32_t(get(b+56)),uint32_t(get(b+60))};
 s.muteRemainingMs=uint32_t(get(b+64));s.crcErrors=uint32_t(get(b+68));s.lengthErrors=uint32_t(get(b+72));s.timeouts=uint32_t(get(b+76));s.recoveries=uint32_t(get(b+80));s.ack={AckResult(b[84]),CommandOp(b[85]),uint32_t(get(b+88)),uint32_t(get(b+92)),uint32_t(get(b+96)),uint32_t(get(b+100))};s.configOrigin=b[104];s.enabledMask=b[105];
 uint8_t canonical[256];if(!encodeBrakeStatus(s,token,canonical,256)||std::memcmp(b,canonical,256))return false;
 out=s;return true;
}
BrakeView updateBrakeView(const BrakeDelivery&d,uint64_t now){
 BrakeView v{};if(now<d.receivedLocalUs||!d.state.brakeBootId)return v;
 const auto local=elapsedMs(now,d.receivedLocalUs);v.stateAgeMs=sumAge(d.state.stateAgeMs,d.requestElapsedMs,local);v.stateFresh=v.stateAgeMs<=1000;
 for(unsigned i=0;i<2;++i){v.sensorAgeMs[i]=sumAge(d.state.sensorAgeMs[i],d.requestElapsedMs,local);v.sensorFresh[i]=(d.state.validMask&(1<<i))&&v.sensorAgeMs[i]<=2500;}
 return v;
}
}
