#pragma once
#include <Arduino.h>
#include <driver/spi_master.h>
#include <WiredTireCore.h>
enum class BoardPeer:uint8_t{TireFL,TireFR,TireRL,TireRR,Brake};
struct BoardBusConfig{bool hardwareEnabled=false;int sck=-1,mosi=-1,miso=-1;std::array<int,5>selects{{-1,-1,-1,-1,-1}},ready{{-1,-1,-1,-1,-1}};unsigned peerCount=5;};
struct BusResult{bool success=false,timedOut=false;size_t length=0;uint64_t elapsedUs=0,completedUs=0;};
class BoardSpiBus {
 public:
  WT_OFFLINE_LINK bool begin(const BoardBusConfig&);
  WT_OFFLINE_LINK void poll(uint64_t now);
  WT_OFFLINE_LINK bool tryQueue(BoardPeer,const uint8_t*,uint8_t*,size_t);
  bool takeCompleted(BoardPeer,BusResult&);
  bool enabled()const{return enabled_;}
 private:
  struct Peer{spi_device_handle_t device=nullptr;spi_transaction_t transaction{};uint8_t*tx=nullptr,*rx=nullptr,*destination=nullptr;int ready=-1;BusResult result{};bool completed=false;};
  std::array<Peer,5>peers_{};bool enabled_=false,late_=false;int active_=-1;uint64_t now_=0,started_=0;
  void cleanup();
};
BoardSpiBus& defaultBoardSpiBus();
