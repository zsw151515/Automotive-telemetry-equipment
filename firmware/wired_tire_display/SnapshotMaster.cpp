#include "SnapshotMaster.h"
#include <esp_system.h>
#include <esp_timer.h>
using namespace wired_tire;
bool SnapshotMaster::begin(){if(!bus_.enabled()||enabled_)return false;token_=esp_random();enabled_=true;return true;}
bool SnapshotMaster::begin(int sck,int mosi,int miso,const std::array<int,4>&cs,const std::array<int,4>&ready){BoardBusConfig c{};c.hardwareEnabled=true;c.sck=sck;c.mosi=mosi;c.miso=miso;c.peerCount=4;for(unsigned i=0;i<4;++i){c.selects[i]=cs[i];c.ready[i]=ready[i];}return bus_.begin(c)&&begin();}
bool SnapshotMaster::pollWheel(WheelId wheel,SnapshotDelivery&out){
 if(!enabled_||!validWheel(wheel))return false;
 const auto now=uint64_t(esp_timer_get_time());bus_.poll(now);const auto i=unsigned(wheel)-1;auto&c=channels_[i];BusResult result{};
 if(bus_.takeCompleted(BoardPeer(i),result)){
  c.inFlight=false;Snapshot decoded{};
  if(result.success&&c.schedule.pending()&&now>=c.started&&now-c.started<100000&&decodeSnapshot(c.rx.data(),1728,wheel,c.token,decoded)==DecodeResult::Ok){c.schedule.response(now,true);out={decoded,now,wired_brake::elapsedMs(now,c.started)};return true;}
 }
 const auto action=c.schedule.poll(now);
 if(action==ProbeAction::Request){c.started=now;c.token=++token_;if(!c.token)c.token=++token_;encodeSnapshotRequest(wheel,c.token,c.tx.data(),1728);}
 if(c.schedule.pending()&&!c.inFlight)c.inFlight=bus_.tryQueue(BoardPeer(i),c.tx.data(),c.rx.data(),1728);
 return false;
}
