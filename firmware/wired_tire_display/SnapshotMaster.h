#pragma once
#include "BoardSpiBus.h"
class SnapshotMaster {
 public:
  explicit SnapshotMaster(BoardSpiBus&bus=defaultBoardSpiBus()):bus_(bus){}
  WT_OFFLINE_LINK bool begin();
  WT_OFFLINE_LINK bool begin(int,int,int,const std::array<int,4>&,const std::array<int,4>&);
  WT_OFFLINE_LINK bool pollWheel(wired_tire::WheelId,wired_tire::SnapshotDelivery&);
 private:
  struct Channel{std::array<uint8_t,1728>tx{},rx{};wired_tire::ProbeSchedule schedule{100000,250000,500000};uint64_t started=0;uint32_t token=0;bool inFlight=false;};
  BoardSpiBus&bus_;std::array<Channel,4>channels_{};uint32_t token_=0;bool enabled_=false;
};
