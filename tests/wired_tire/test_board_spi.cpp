#include "board_test_support.h"
#include <cstring>
#include "SnapshotMaster.h"
#include <esp_timer.h>
int main(){
 runTest("snapshot_master_wrong_token_then_peer_reboot",[]{BoardSpiBus bus;CHECK(bus.begin(busProfile()));SnapshotMaster master(bus);CHECK(master.begin());wired_tire::SnapshotDelivery out{};out.snapshot.meta.bootId=99;test_hw::clockUs=0;CHECK(!master.pollWheel(wired_tire::WheelId::FL,out));
  auto*t=test_hw::masterTransaction;uint32_t token=0;CHECK(wired_tire::decodeSnapshotRequest(static_cast<const uint8_t*>(t->tx_buffer),1728,wired_tire::WheelId::FL,token));
  wired_tire::ThermalFrame f{};wired_tire::FrameMeta meta{};meta.wheel=wired_tire::WheelId::FL;meta.bootId=2;wired_tire::ZoneResult zones{};wired_tire::LinkStats stats{};
  CHECK(wired_tire::encodeSnapshot(f,meta,zones,stats,0,token+1,static_cast<uint8_t*>(t->rx_buffer),1728)==wired_tire::EncodeResult::Ok);test_hw::masterDone=true;test_hw::clockUs=1000;CHECK(!master.pollWheel(wired_tire::WheelId::FL,out)&&out.snapshot.meta.bootId==99);
  t=test_hw::masterTransaction;CHECK(wired_tire::encodeSnapshot(f,meta,zones,stats,0,token,static_cast<uint8_t*>(t->rx_buffer),1728)==wired_tire::EncodeResult::Ok);test_hw::masterDone=true;test_hw::clockUs=2000;CHECK(master.pollWheel(wired_tire::WheelId::FL,out)&&out.snapshot.meta.bootId==2);
 });
 runTest("mixed_five_peers_and_ready_low_does_not_starve",[]{BoardSpiBus bus;auto c=busProfile();CHECK(bus.begin(c));std::array<uint8_t,1728>tx{},rx{};BusResult result{};
  for(unsigned i=0;i<5;++i){const size_t size=i==4?256:1728;bus.poll(i*2000);CHECK(bus.tryQueue(BoardPeer(i),tx.data(),rx.data(),size));CHECK(test_hw::masterTransaction->length==size*8);std::memset(test_hw::masterTransaction->rx_buffer,0x5a,size);test_hw::masterDone=true;bus.poll(i*2000+1000);CHECK(bus.takeCompleted(BoardPeer(i),result)&&result.success&&rx[size-1]==0x5a);}
  test_hw::levels[c.ready[0]]=0;CHECK(!bus.tryQueue(BoardPeer::TireFL,tx.data(),rx.data(),1728));CHECK(bus.tryQueue(BoardPeer::TireFR,tx.data(),rx.data(),1728));
 });
 runTest("dma_timeout_retains_lease_until_actual_completion",[]{BoardSpiBus bus;CHECK(bus.begin(busProfile()));std::array<uint8_t,1728>tx{},rx{};tx[0]=9;bus.poll(0);CHECK(bus.tryQueue(BoardPeer::TireFL,tx.data(),rx.data(),1728));const auto*lease=static_cast<const uint8_t*>(test_hw::masterTransaction->tx_buffer);
  bus.poll(100000);tx[0]=4;CHECK(!bus.tryQueue(BoardPeer::Brake,tx.data(),rx.data(),256)&&lease[0]==9);
  test_hw::masterDone=true;bus.poll(100001);BusResult r{};CHECK(bus.takeCompleted(BoardPeer::TireFL,r)&&r.timedOut&&!r.success);CHECK(bus.tryQueue(BoardPeer::Brake,tx.data(),rx.data(),256));
 });
}
