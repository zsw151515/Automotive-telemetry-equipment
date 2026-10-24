#pragma once
#include <array>
#include <cstdint>
#include <limits>
namespace wired_brake {
constexpr int16_t kInvalidTemperature=0x7FFF;
constexpr uint32_t kUnknownAge=UINT32_MAX;
struct BrakeConfig { uint16_t alarmC=600; bool warningBeep=true; };
inline bool operator==(const BrakeConfig&a,const BrakeConfig&b) {return a.alarmC==b.alarmC&&a.warningBeep==b.warningBeep;}
inline bool validConfig(const BrakeConfig& c) {return c.alarmC>=100&&c.alarmC<=900&&c.alarmC%10==0;}
struct BrakeReading { int16_t temp10=kInvalidTemperature; bool valid=false; uint64_t receivedUs=0; };
enum class BrakeState:uint8_t {Ok=0,Warn=1,Alarm=2,Fail=3};
struct BrakeModelResult {
  int16_t max10=kInvalidTemperature,fast10=kInvalidTemperature,core10=kInvalidTemperature;
  uint16_t dose10=0; bool anyFresh=false,modelValid=false;uint8_t failMask=3;BrakeState state=BrakeState::Fail;
};
enum class CommandOp:uint8_t {Read=0,ApplySave=1,Mute60=2};
enum class AckResult:uint8_t {None,Pending,AppliedSaved,Muted,Invalid,StaleBoot,VersionConflict,IdConflict,StorageFailed,OutcomeUnknown,SessionMismatch,Busy};
struct BrakeCommand {
  CommandOp op=CommandOp::Read;uint32_t hostBootId=0,targetBrakeBootId=0,commandId=0,expectedConfigVersion=0;
  BrakeConfig candidate{};uint32_t muteMs=0;
};
struct ControlIntent {CommandOp op=CommandOp::Read;BrakeConfig candidate{};};
struct BrakeAck {AckResult result=AckResult::None;CommandOp op=CommandOp::Read;
  uint32_t hostBootId=0,commandId=0,targetBrakeBootId=0,configVersion=0;};
struct BrakeTelemetry {
  BrakeState state=BrakeState::Fail;
  bool bleEnabled=false,bleConnected=false,configValid=false,modelValid=false,saved=false,recovered=false;
  uint32_t brakeBootId=0,sequence=0,stateAgeMs=kUnknownAge;uint64_t sourceStateUs=0;
  uint32_t configVersion=0,savedVersion=0;BrakeConfig config{};uint8_t validMask=0;
  std::array<int16_t,2> temp10{{kInvalidTemperature,kInvalidTemperature}};
  int16_t max10=kInvalidTemperature,fast10=kInvalidTemperature,core10=kInvalidTemperature;uint16_t dose10=0;
  std::array<uint32_t,2> sensorAgeMs{{kUnknownAge,kUnknownAge}};
  uint32_t muteRemainingMs=0,crcErrors=0,lengthErrors=0,timeouts=0,recoveries=0;
  BrakeAck ack{};uint8_t configOrigin=0,enabledMask=3;
};
inline uint32_t elapsedMs(uint64_t now,uint64_t then) {
  if(now<then)return kUnknownAge;
  const uint64_t d=now-then,ms=d/1000+(d%1000!=0);
  return ms>=UINT32_MAX?UINT32_MAX:static_cast<uint32_t>(ms);
}
inline uint64_t deadlineAfter(uint64_t now,uint64_t delta){return now>UINT64_MAX-delta?UINT64_MAX:now+delta;}
}
