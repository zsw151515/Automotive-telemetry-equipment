#include "brake_test_support.h"
int main(){
 runTest("save_then_apply_once_and_lost_ack_retry_no_extra_flash_or_reset",[]{
  CountingStore s;BrakeModel m;BrakeBuzzer b;BrakeControl ctl(s,m,b);CHECK(s.loads==0);ctl.begin(11,0);ctl.handle(readCommand(),0);
  auto c=saveCommand();CHECK(ctl.handle(c,1).result==AckResult::AppliedSaved);CHECK(ctl.configVersion()==1&&ctl.config().alarmC==650&&s.commitCount==1);
  CHECK(ctl.handle(c,99).result==AckResult::AppliedSaved&&s.commitCount==1);
  c.commandId=2;c.expectedConfigVersion=1;CHECK(ctl.handle(c,100).result==AckResult::AppliedSaved&&s.commitCount==1);
 });
 runTest("same_alarm_does_not_reset_on_warning_toggle",[]{CountingStore s;BrakeModel m;BrakeBuzzer b;BrakeControl ctl(s,m,b);ctl.begin(11,0);ctl.handle(readCommand(),0);
  std::array<BrakeReading,2> r{};for(uint64_t t=0;t<=5900000;t+=100000){r[0]={6100,true,t};m.tick(r,1,ctl.config(),t);}
  auto c=saveCommand();c.candidate={600,false};ctl.handle(c,5900001);r[0].receivedUs=6000000;CHECK(m.tick(r,1,ctl.config(),6000000).state==BrakeState::Alarm);
 });
 runTest("mute_retry_does_not_extend_and_evicted_id_is_rejected",[]{CountingStore s;BrakeModel m;BrakeBuzzer b;BrakeControl ctl(s,m,b);ctl.begin(11,0);ctl.handle(readCommand(),0);
  auto c=saveCommand();c.op=CommandOp::Mute60;c.muteMs=60000;CHECK(ctl.handle(c,0).result==AckResult::Muted);
  CHECK(ctl.handle(c,59999000).result==AckResult::Muted&&b.remainingMs(60000000)==0&&s.commitCount==0);
  for(unsigned i=2;i<=10;++i){c.commandId=i;ctl.handle(c,60000000+i);}
  c.commandId=1;CHECK(ctl.handle(c,60001000).result==AckResult::IdConflict);
 });
 runTest("wrong_boot_session_version_or_same_id_different_payload_no_side_effect",[]{CountingStore s;BrakeModel m;BrakeBuzzer b;BrakeControl ctl(s,m,b);ctl.begin(11,0);auto c=saveCommand();
  CHECK(ctl.handle(c,0).result==AckResult::SessionMismatch);ctl.handle(readCommand(),0);
  c.targetBrakeBootId=12;CHECK(ctl.handle(c,0).result==AckResult::StaleBoot);c=saveCommand(2,42);CHECK(ctl.handle(c,0).result==AckResult::VersionConflict);
  c=saveCommand(3);CHECK(ctl.handle(c,0).result==AckResult::AppliedSaved);c.candidate.alarmC=700;CHECK(ctl.handle(c,0).result==AckResult::IdConflict);
  ctl.handle(readCommand(8),0);c=saveCommand(4,1);CHECK(ctl.handle(c,0).result==AckResult::SessionMismatch);CHECK(s.commitCount==1);
 });
 runTest("torn_record_loads_only_complete_old_or_new",[]{PersistRecord a{{600,true},1},b{{650,false},2};std::array<uint8_t,32>x{},y{};CHECK(encodePersist(a,x.data(),32)&&encodePersist(b,y.data(),32));
  PersistRecord out{};bool recovered=false;
  for(size_t n=0;n<=32;++n){auto torn=x;std::copy_n(y.begin(),n,torn.begin());CHECK(selectPersist(x.data(),torn.data(),out,recovered)==LoadResult::Valid);CHECK(out.config.alarmC==(n==32?650:600));}
  auto collision=x;PersistRecord c{{700,true},1};encodePersist(c,collision.data(),32);CHECK(selectPersist(x.data(),collision.data(),out,recovered)==LoadResult::Invalid&&recovered);
 });
 runTest("unknown_commit_is_not_claimed_rollback_and_busy_not_deferred",[]{CountingStore s;s.commitResult=StoreResult::Unknown;s.writeOnUnknown=true;s.readResult=LoadResult::Unavailable;
  BrakeModel m;BrakeBuzzer b;BrakeControl ctl(s,m,b);ctl.begin(11,0);ctl.handle(readCommand(),0);auto c=saveCommand();
  CHECK(ctl.handle(c,1).result==AckResult::OutcomeUnknown&&ctl.config().alarmC==600);CHECK(ctl.handle(readCommand(),2).result==AckResult::OutcomeUnknown);
  auto next=saveCommand(2);next.candidate.alarmC=700;CHECK(ctl.handle(next,3).result==AckResult::Busy);ctl.poll(100000);CHECK(ctl.config().alarmC==600);
  s.readResult=LoadResult::Valid;ctl.poll(200000);CHECK(ctl.config().alarmC==650&&ctl.configVersion()==1&&s.commitCount==1);
  CHECK(ctl.handle(next,200001).result==AckResult::Busy&&s.commitCount==1);CHECK(ctl.handle(c,200002).result==AckResult::AppliedSaved);
 });
}
