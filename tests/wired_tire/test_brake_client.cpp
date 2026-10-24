#include "board_test_support.h"
#include "BrakeLinkMaster.h"
#include <cstring>
static BrakeCommand queued(){BrakeCommand c{};uint32_t t=0;CHECK(decodeBrakeRequest(static_cast<const uint8_t*>(test_hw::masterTransaction->tx_buffer),256,c,t));return c;}
static void reply(BrakeLinkMaster&client,BrakeTelemetry s,uint64_t now){BrakeDelivery out{};client.poll(now,out);BrakeCommand c{};uint32_t token=0;auto*t=test_hw::masterTransaction;CHECK(decodeBrakeRequest(static_cast<const uint8_t*>(t->tx_buffer),256,c,token));CHECK(encodeBrakeStatus(s,token,static_cast<uint8_t*>(t->rx_buffer),256));test_hw::masterDone=true;CHECK(client.poll(now+1000,out));}
int main(){
 runTest("timeout_inside_retry_window_stops_all_future_mutations",[]{BoardSpiBus bus;CHECK(bus.begin(busProfile()));BrakeLinkMaster client(bus);auto s=validBrakeTelemetry();reply(client,s,0);CHECK(client.submit({CommandOp::ApplySave,{650,true}})==SubmitResult::Accepted);reply(client,s,2000);reply(client,s,500000);
  BrakeDelivery out{};client.poll(950000,out);CHECK(queued().op==CommandOp::ApplySave);std::memset(test_hw::masterTransaction->rx_buffer,0,256);test_hw::masterDone=true;client.poll(1001001,out);CHECK(client.pendingResult().result==AckResult::OutcomeUnknown);CHECK(queued().op==CommandOp::Read);
 });
 runTest("not_ready_and_matching_saved_ack",[]{BoardSpiBus bus;BrakeLinkMaster client(bus);ControlIntent intent{CommandOp::ApplySave,{650,true}};CHECK(client.submit(intent)==SubmitResult::NotReady);CHECK(bus.begin(busProfile()));auto s=validBrakeTelemetry();reply(client,s,0);
  CHECK(client.submit(intent)==SubmitResult::Accepted&&client.submit(intent)==SubmitResult::Busy);BrakeDelivery out{};client.poll(2000,out);auto c=queued();CHECK(c.op==CommandOp::ApplySave&&c.expectedConfigVersion==1&&c.targetBrakeBootId==1&&c.hostBootId&&c.commandId);
  s.config={650,true};s.configVersion=s.savedVersion=2;s.ack={AckResult::AppliedSaved,c.op,c.hostBootId,c.commandId,1,2};reply(client,s,3000);CHECK(client.pendingResult().result==AckResult::AppliedSaved);
 });
 runTest("ack_lost_reconnect_reads_actual_never_replays_mutation",[]{BoardSpiBus bus;CHECK(bus.begin(busProfile()));BrakeLinkMaster client(bus);auto s=validBrakeTelemetry();reply(client,s,0);CHECK(client.submit({CommandOp::ApplySave,{650,true}})==SubmitResult::Accepted);
  reply(client,s,2000);auto original=queued();for(uint64_t t:{253000ull,504000ull,755000ull}){reply(client,s,t);CHECK(queued().commandId==original.commandId);}
  BrakeDelivery out{};client.poll(1002001,out);CHECK(client.pendingResult().result==AckResult::OutcomeUnknown);CHECK(queued().op==CommandOp::Read);
 });
 runTest("peer_reboot_drops_old_write_intent",[]{BoardSpiBus bus;CHECK(bus.begin(busProfile()));BrakeLinkMaster client(bus);auto s=validBrakeTelemetry();reply(client,s,0);CHECK(client.submit({CommandOp::Mute60,{}})==SubmitResult::Accepted);s.brakeBootId=2;reply(client,s,2000);CHECK(client.pendingResult().result==AckResult::OutcomeUnknown);BrakeDelivery out{};client.poll(254000,out);CHECK(queued().op==CommandOp::Read);});
}
