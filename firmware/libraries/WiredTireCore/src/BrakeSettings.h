#pragma once
#include "BrakeModel.h"
#include "BrakeBuzzer.h"
#include <cstddef>
namespace wired_brake {
struct PersistRecord {BrakeConfig config{};uint32_t version=0;uint8_t origin=1;};
enum class LoadResult {Valid,Missing,Invalid,Unavailable};
enum class StoreResult {Committed,NotCommitted,Unknown};
bool encodePersist(const PersistRecord&,uint8_t*,size_t);
bool decodePersist(const uint8_t*,size_t,PersistRecord&);
LoadResult selectPersist(const uint8_t* slot0,const uint8_t* slot1,PersistRecord&,bool& recovered);
class ConfigStore {
 public:virtual ~ConfigStore()=default;
  virtual LoadResult load(PersistRecord&)=0;
  virtual StoreResult commit(const PersistRecord&)=0;
  // Authoritative complete old/new record after commit has returned. No delayed writes remain.
  virtual LoadResult readBack(uint32_t version,PersistRecord&)=0;
};
class BrakeControl {
 public:
  BrakeControl(ConfigStore&s,BrakeModel&m,BrakeBuzzer&b):store_(s),model_(m),buzzer_(b){}
  void begin(uint32_t brakeBoot,uint64_t now);
  BrakeAck handle(const BrakeCommand&,uint64_t now);
  void poll(uint64_t now);
  const BrakeConfig& config()const{return effective_.config;}
  uint32_t configVersion()const{return effective_.version;}
  uint32_t savedVersion()const{return saved_?effective_.version:0;}
  bool saved()const{return saved_;}bool recovered()const{return recovered_;}
  uint8_t origin()const{return effective_.origin;}
  BrakeAck ack()const{return ack_;}
 private:
  struct Entry{bool valid=false;BrakeCommand command{};BrakeAck ack{};};
  ConfigStore&store_;BrakeModel&model_;BrakeBuzzer&buzzer_;
  PersistRecord effective_{},pendingRecord_{};BrakeCommand pendingCommand_{};BrakeAck ack_{};
  uint32_t boot_=0,host_=0,highWater_=0;bool begun_=false,saved_=false,recovered_=false,unknown_=false;
  uint64_t nextRead_=0;std::array<Entry,8> cache_{};size_t next_=0;
  BrakeAck makeAck(const BrakeCommand&,AckResult)const;
  BrakeAck remember(const BrakeCommand&,AckResult);
  void activate(const PersistRecord&,uint64_t now);
};
}
