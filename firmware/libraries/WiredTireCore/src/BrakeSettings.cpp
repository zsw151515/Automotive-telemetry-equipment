#include "BrakeSettings.h"
#include "SnapshotWire.h"
#include <algorithm>
#include <cstring>
namespace wired_brake {
namespace {
void put32(uint8_t*p,uint32_t v){for(unsigned i=0;i<4;++i)p[i]=uint8_t(v>>(8*i));}
uint32_t get32(const uint8_t*p){uint32_t v=0;for(unsigned i=0;i<4;++i)v|=uint32_t(p[i])<<(8*i);return v;}
bool same(const BrakeCommand&a,const BrakeCommand&b){return a.op==b.op&&a.hostBootId==b.hostBootId&&a.targetBrakeBootId==b.targetBrakeBootId&&a.commandId==b.commandId&&a.expectedConfigVersion==b.expectedConfigVersion&&(a.op!=CommandOp::ApplySave||a.candidate==b.candidate)&&(a.op!=CommandOp::Mute60||a.muteMs==b.muteMs);}
}
bool encodePersist(const PersistRecord&r,uint8_t*out,size_t n){
 if(!out||n!=32||!validConfig(r.config)||!r.version)return false;
 std::memset(out,0,n);std::memcpy(out,"BCF1",4);out[4]=1;out[5]=r.config.warningBeep;out[6]=uint8_t(r.config.alarmC);out[7]=uint8_t(r.config.alarmC>>8);put32(out+8,r.version);put32(out+28,wired_tire::wireCrc32(out,28));return true;
}
bool decodePersist(const uint8_t*b,size_t n,PersistRecord&out){
 if(!b||n!=32||std::memcmp(b,"BCF1",4)||b[4]!=1||b[5]>1||get32(b+28)!=wired_tire::wireCrc32(b,28))return false;
 for(size_t i=12;i<28;++i)if(b[i])return false;
 PersistRecord r{{uint16_t(b[6]|uint16_t(b[7])<<8),b[5]!=0},get32(b+8)};
 if(!validConfig(r.config)||!r.version)return false;
 out=r;return true;
}
LoadResult selectPersist(const uint8_t*a,const uint8_t*b,PersistRecord&out,bool& recovered){
 PersistRecord x{},y{};const bool va=decodePersist(a,32,x),vb=decodePersist(b,32,y);recovered=false;
 if(!va&&!vb){recovered=(a||b);return (a||b)?LoadResult::Invalid:LoadResult::Missing;}
 if(va&&vb&&x.version==y.version&&!(x.config==y.config)){recovered=true;return LoadResult::Invalid;}
 recovered=(a&&!va)||(b&&!vb);out=!va?y:!vb?x:x.version>=y.version?x:y;return LoadResult::Valid;
}
void BrakeControl::begin(uint32_t boot,uint64_t now){
 boot_=boot;host_=highWater_=0;cache_={};next_=0;unknown_=false;ack_={};effective_={};effective_.origin=0;saved_=recovered_=false;begun_=boot!=0;
 retiredHosts_={};retiredCount_=0;
 PersistRecord r{};const auto load=store_.load(r);
 if(load==LoadResult::Valid&&validConfig(r.config)){effective_=r;saved_=r.origin==1&&r.version>0;recovered_=r.recovered||r.origin!=1;}
 else recovered_=load!=LoadResult::Missing;
 model_.reset(now);buzzer_.begin(now);
}
BrakeAck BrakeControl::makeAck(const BrakeCommand&c,AckResult r)const{return {r,c.op,c.hostBootId,c.commandId,boot_,effective_.version};}
BrakeAck BrakeControl::remember(const BrakeCommand&c,AckResult result){
 BrakeAck a=makeAck(c,result);
 if(c.hostBootId==host_){
  for(auto&e:cache_)if(e.valid&&e.command.commandId==c.commandId){e.ack=a;if(!unknown_)ack_=a;return a;}
  cache_[next_]={true,c,a};next_=(next_+1)%cache_.size();
 }
 if(!unknown_||result==AckResult::OutcomeUnknown)ack_=a;
 return a;
}
void BrakeControl::activate(const PersistRecord&r,uint64_t now){
 const bool changed=effective_.config.alarmC!=r.config.alarmC;effective_=r;saved_=true;recovered_=false;
 if(changed)model_.reset(now);
}
BrakeAck BrakeControl::handle(const BrakeCommand&c,uint64_t now){
 if(!begun_||!c.hostBootId)return makeAck(c,AckResult::Invalid);
 if(c.op==CommandOp::Read){
  if(c.targetBrakeBootId&&c.targetBrakeBootId!=boot_)return makeAck(c,AckResult::StaleBoot);
  if(host_!=c.hostBootId){
   for(size_t i=0;i<retiredCount_;++i)if(retiredHosts_[i]==c.hostBootId)return makeAck(c,AckResult::SessionMismatch);
   // Never evict replay protection. At capacity require a brake reboot before accepting another host.
   if(host_){if(retiredCount_==retiredHosts_.size())return makeAck(c,AckResult::SessionMismatch);retiredHosts_[retiredCount_++]=host_;}
   host_=c.hostBootId;highWater_=0;cache_={};next_=0;
  }
  return ack_;
 }
 if(c.hostBootId!=host_)return makeAck(c,AckResult::SessionMismatch);
 if(!c.commandId)return makeAck(c,AckResult::Invalid);
 for(const auto&e:cache_)if(e.valid&&e.command.commandId==c.commandId)return same(c,e.command)?e.ack:makeAck(c,AckResult::IdConflict);
 if(c.commandId<=highWater_)return makeAck(c,AckResult::IdConflict);
 highWater_=c.commandId;
 if(c.targetBrakeBootId!=boot_)return remember(c,AckResult::StaleBoot);
 if(unknown_)return remember(c,AckResult::Busy);
 if(c.expectedConfigVersion!=effective_.version)return remember(c,AckResult::VersionConflict);
 if(c.op==CommandOp::Mute60){if(c.muteMs!=60000)return remember(c,AckResult::Invalid);buzzer_.mute60(now);return remember(c,AckResult::Muted);}
 if(c.op!=CommandOp::ApplySave||!validConfig(c.candidate))return remember(c,AckResult::Invalid);
 if(saved_&&c.candidate==effective_.config)return remember(c,AckResult::AppliedSaved);
 if(effective_.version==UINT32_MAX)return remember(c,AckResult::VersionConflict);
 pendingRecord_={c.candidate,effective_.version+1};pendingCommand_=c;
 const auto result=store_.commit(pendingRecord_);
 if(result==StoreResult::Committed){activate(pendingRecord_,now);return remember(c,AckResult::AppliedSaved);}
 if(result==StoreResult::NotCommitted)return remember(c,AckResult::StorageFailed);
 unknown_=true;nextRead_=deadlineAfter(now,100000);return remember(c,AckResult::OutcomeUnknown);
}
void BrakeControl::poll(uint64_t now){
 if(!unknown_||now<nextRead_)return;
 nextRead_=deadlineAfter(now,100000);PersistRecord r{};const auto result=store_.readBack(pendingRecord_.version,r);
 if(result==LoadResult::Unavailable||result==LoadResult::Invalid)return;
 if(result==LoadResult::Valid&&(!validConfig(r.config)||r.version>pendingRecord_.version))return;
 unknown_=false;
 if(result==LoadResult::Valid&&r.version==pendingRecord_.version&&r.config==pendingRecord_.config){activate(r,now);remember(pendingCommand_,AckResult::AppliedSaved);}
 else if(result==LoadResult::Missing||(result==LoadResult::Valid&&r.version==effective_.version&&r.config==effective_.config))remember(pendingCommand_,AckResult::StorageFailed);
 else{unknown_=true;ack_=makeAck(pendingCommand_,AckResult::OutcomeUnknown);}
}
}
