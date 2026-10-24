#include "brake_test_support.h"
#include "BrakeHardwareStub.h"
#include "DualBrakeProbe.h"
#include "BrakeBle.h"
#include "BrakeNvsStore.h"
#include "BrakeSpiSlave.h"
#include <nvs.h>
#include <cstring>
static BrakeHardwareConfig profile(){BrakeHardwareConfig c{};c.hardwareEnabled=c.approved=true;c.probes={{{0,1,2,1,true},{3,4,5,2,true}}};c.sck=6;c.mosi=7;c.miso=8;c.select=9;c.ready=10;c.buzzerPin=20;return c;}
static std::vector<uint8_t> response(uint8_t addr,int value){std::vector<uint8_t>b{addr,3,2,uint8_t(value>>8),uint8_t(value),0,0};repairFixtureCrc(b);return b;}
int main(){
 runTest("invalid_length_slot_recovers_other_complete_slot",[]{nvs_mock::values.clear();BrakeNvsStore store;CHECK(store.begin(profile()));CHECK(store.commit({{600,true},1})==StoreResult::Committed);CHECK(store.commit({{650,true},2})==StoreResult::Committed);
  nvs_mock::values["1/slot1"]={1};PersistRecord out{};CHECK(store.load(out)==LoadResult::Valid&&out.version==2&&out.recovered);
 });
 runTest("nvs_dual_slots_and_unknown_readback",[]{nvs_mock::values.clear();nvs_mock::writes=0;BrakeNvsStore store;CHECK(store.begin(profile()));PersistRecord out{};CHECK(store.load(out)==LoadResult::Missing);
  CHECK(store.commit({{650,true},1})==StoreResult::Committed);CHECK(store.commit({{700,false},2})==StoreResult::Committed);CHECK(store.load(out)==LoadResult::Valid&&out.version==2);
  nvs_mock::failAfterCommit=true;CHECK(store.commit({{750,true},3})==StoreResult::Unknown);nvs_mock::failRead=false;nvs_mock::failAfterCommit=false;
  CHECK(store.readBack(3,out)==LoadResult::Valid&&out.config.alarmC==750&&nvs_mock::writes==3);
 });
 runTest("spi_publish_cannot_mutate_dma_lease",[]{BrakeSpiSlave spi;CHECK(!spi.begin({}));CHECK(spi.begin(profile()));auto s=validBrakeTelemetry();s.brakeBootId=11;spi.publish(s);spi.poll(0);
  auto*t=test_hw::slaveTransaction;auto tx=static_cast<const uint8_t*>(t->tx_buffer);std::vector<uint8_t> before(tx,tx+256);spi.publish(s);spi.poll(1000000);CHECK(std::equal(before.begin(),before.end(),tx));
  std::array<uint8_t,256>request{};encodeBrakeRequest(readCommand(),1,request.data(),256);std::memcpy(t->rx_buffer,request.data(),256);t->trans_len=2048;test_hw::slaveDone=true;spi.poll(1000001);
  BrakeCommand c{};CHECK(spi.takeCommand(c)&&c.op==CommandOp::Read);BrakeTelemetry out{};CHECK(decodeBrakeStatus(static_cast<const uint8_t*>(t->tx_buffer),256,1,out)&&out.stateAgeMs==1001);
 });
 runTest("unapproved_or_usb_conflicting_profile_touches_no_uart",[]{DualBrakeProbe p;test_hw::uartBegins=0;CHECK(!p.begin({})&&test_hw::uartBegins==0);auto c=profile();c.probes[1].tx=19;CHECK(!p.begin(c)&&test_hw::uartBegins==0);});
 runTest("late_tail_and_noise_do_not_starve_other_bus",[]{for(auto& s:test_hw::uartPorts)s={};DualBrakeProbe p;CHECK(p.begin(profile()));p.poll(0);test_hw::uartPorts[0].inject(response(1,6100));p.poll(20000);CHECK(!p.readings()[0].valid);
  test_hw::uartPorts[0].inject(std::vector<uint8_t>(1024,0xff));p.poll(100000);CHECK(test_hw::uartPorts[1].tx.size()==8);test_hw::uartPorts[1].inject(response(2,-123));p.poll(110000);CHECK(p.readings()[1].valid&&p.readings()[1].temp10==-123&&p.readings()[1].receivedUs==110000);
 });
 runTest("display_absent_ble_disconnected_alarm_continues",[]{for(auto& s:test_hw::uartPorts)s={};DualBrakeProbe p;CHECK(p.begin(profile()));BrakeBle ble;CHECK(ble.begin(profile()));ble.onConnect(nullptr);ble.onDisconnect(nullptr);BrakeModel m;BrakeBuzzer buzzer;buzzer.begin(0);
  for(uint64_t t=0;t<=6000000;t+=10000){p.poll(t);if(t%200000==0)test_hw::uartPorts[0].inject(response(1,6100));if(t%200000==100000)test_hw::uartPorts[1].inject(response(2,6100));m.tick(p.readings(),3,{},t);ble.poll(t,validBrakeTelemetry());}
  p.poll(6010000);m.tick(p.readings(),3,{},6010000);CHECK(m.result().state==BrakeState::Alarm&&buzzer.output(m.result().state,false,6010000));
 });
}
