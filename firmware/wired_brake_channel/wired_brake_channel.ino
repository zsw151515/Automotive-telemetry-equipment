// 六接口底板第五C3：两前刹车独立RS485、V1.6模型、蜂鸣、NVS、独立RaceChrono BLE。
#include <Arduino.h>
#include <esp_timer.h>
#include <esp_system.h>
#include "DualBrakeProbe.h"
#include "BrakeNvsStore.h"
#include "BrakeSpiSlave.h"
#include "BrakeBle.h"
#if !CONFIG_IDF_TARGET_ESP32C3
#error "wired_brake_channel requires ESP32-C3"
#endif
#if !ARDUINO_USB_CDC_ON_BOOT
#error "Brake dual UART requires CDCOnBoot=cdc; UART0 is not the debug console"
#endif
using namespace wired_brake;
DualBrakeProbe probes;BrakeNvsStore store;BrakeModel model;BrakeBuzzer buzzer;
BrakeControl control(store,model,buzzer);BrakeBle ble;BrakeSpiSlave spi;
bool running=false;uint32_t bootId=0,sequence=0;uint64_t logAt=0;
void setup(){
 Serial.begin(115200);Serial.println("Boot six-port brake C3 0.2.0 / OFFLINE VALIDATED ONLY");
 const auto&c=brake_config::hardware;
 Serial.printf("Board=C3 dual brake; SPI SCK=%d MOSI=%d MISO=%d CS=%d READY=%d buzzer=%d USB=18/19\n",c.sck,c.mosi,c.miso,c.select,c.ready,c.buzzerPin);
 Serial.println("Supply=external protected ignition source; supply voltage not measured. DE must be held OFF in hardware at boot.");
 if(!validBrakeHardware(c)){Serial.println("No approved profile; UART/NVS/BLE/SPI/buzzer IO disabled");return;}
 pinMode(c.buzzerPin,OUTPUT);digitalWrite(c.buzzerPin,LOW);
 if(!store.begin(c))return;
 bootId=esp_random();if(!bootId)bootId=1;control.begin(bootId,uint64_t(esp_timer_get_time()));
 if(!probes.begin(c))Serial.println("Brake probes initialization FAILED; FAIL remains visible");
 if(!ble.begin(c))Serial.println("Brake BLE initialization FAILED");
 if(!spi.begin(c))Serial.println("Brake SPI initialization FAILED; local alarms remain independent");
 running=true;
}
void loop(){
 const auto now=uint64_t(esp_timer_get_time());
 if(running){
  probes.poll(now);spi.poll(now);BrakeCommand cmd{};
  if(spi.takeCommand(cmd)){const auto a=control.handle(cmd,now);if(cmd.op!=CommandOp::Read)Serial.printf("Brake command=%lu result=%u cfg=%lu\n",static_cast<unsigned long>(cmd.commandId),unsigned(a.result),static_cast<unsigned long>(control.configVersion()));}
  control.poll(now);
  // Re-read monotonic time after possible bounded-but-blocking Flash IO; never hide that pause.
  const auto serviced=uint64_t(esp_timer_get_time());probes.poll(serviced);
  const auto m=model.tick(probes.readings(),probes.enabledMask(),control.config(),serviced);
  BrakeTelemetry s{};s.brakeBootId=bootId;s.sequence=++sequence;s.state=m.state;s.sourceStateUs=serviced;s.stateAgeMs=0;
  s.config=control.config();s.configVersion=control.configVersion();s.savedVersion=control.savedVersion();s.saved=control.saved();s.recovered=control.recovered();s.configOrigin=control.origin();s.configValid=true;
  s.enabledMask=probes.enabledMask();s.validMask=s.enabledMask&~m.failMask;s.modelValid=m.modelValid;s.max10=m.max10;s.fast10=m.fast10;s.core10=m.core10;s.dose10=m.dose10;
  for(unsigned i=0;i<2;++i){s.temp10[i]=probes.readings()[i].temp10;s.sensorAgeMs[i]=probes.readings()[i].valid?elapsedMs(serviced,probes.readings()[i].receivedUs):UINT32_MAX;}
  s.ack=control.ack();s.bleEnabled=ble.enabled();s.bleConnected=ble.connected();const auto stats=probes.stats();s.crcErrors=stats.crcErrors;s.lengthErrors=stats.lengthErrors;s.timeouts=stats.timeouts;s.recoveries=stats.recoveries;
  digitalWrite(brake_config::hardware.buzzerPin,buzzer.output(m.state,s.config.warningBeep,serviced)?HIGH:LOW);s.muteRemainingMs=buzzer.remainingMs(serviced);
  ble.poll(serviced,s);spi.publish(s);
  if(serviced>=logAt){logAt=deadlineAfter(serviced,1000000);Serial.printf("Brake state=%u valid=%u FL=%d FR=%d core=%d dose=%u ALM=%u saved=%u BLE=%u notify=%lu timeout=%lu NVS_pause_us=%llu\n",unsigned(s.state),s.validMask,s.temp10[0],s.temp10[1],s.core10,s.dose10,s.config.alarmC,s.saved,s.bleConnected,static_cast<unsigned long>(ble.attempts()),static_cast<unsigned long>(s.timeouts),static_cast<unsigned long long>(store.maxPauseUs()));}
 }
 yield();
}
