// 微雪7B套件＋六接口底板；默认全部I/O禁用。
#include <Arduino.h>
#include <esp_timer.h>
#include "DisplayConfig.h"
#include "Board7BPanel.h"
#include "Board7BTouch.h"
#include "SnapshotMaster.h"
#include "BrakeLinkMaster.h"
#include "HeatmapRenderer.h"
#if !CONFIG_IDF_TARGET_ESP32P4
#error "Requires ESP32-P4"
#endif
BoardSpiBus boardBus;SnapshotMaster tires(boardBus);BrakeLinkMaster brakes(boardBus);
Board7BPanel panel;Board7BTouch touchPanel;HeatmapRenderer renderer;wired_brake::TelemetryUi ui;
std::array<wired_tire::SnapshotDelivery,4> deliveries{};wired_tire::SnapshotDelivery tireIncoming{};wired_brake::BrakeDelivery brakeIncoming{};
bool running=false;uint64_t nextLog=0;unsigned cursor=0;
void setup(){
 Serial.begin(115200);Serial.println("Boot six-port 7B display 0.2.0 / OFFLINE VALIDATED ONLY");
 Serial.println("Board=Waveshare ESP32-P4-WIFI6-Touch-LCD-7B 1024x600; protected ignition supply, voltage not measured");
 const auto&p=display_config::board;if(!approved7B(p)){Serial.println("Unapproved panel/touch/13 GPIO/strap/VO4 profile; all IO disabled");return;}
 BoardBusConfig c{};c.hardwareEnabled=true;c.sck=p.pins.bus[0];c.mosi=p.pins.bus[1];c.miso=p.pins.bus[2];c.selects=p.pins.cs;c.ready=p.pins.ready;
 for(unsigned i=0;i<5;++i)Serial.printf("Peer %u CS=%d READY=%d\n",i,c.selects[i],c.ready[i]);
 Serial.printf("SPI SCK=%d MOSI=%d MISO=%d; LCD BL=32 RST=33; GT911 SDA=7 SCL=8\n",c.sck,c.mosi,c.miso);
 if(boardBus.begin(c))tires.begin();else Serial.println("SPI init FAILED");
 if(!panel.begin(p))Serial.println("Panel init FAILED; background acquisition continues");
 if(!touchPanel.begin(p))Serial.println("Touch init FAILED; background acquisition continues");running=true;
}
void loop(){
 const auto now=uint64_t(esp_timer_get_time());
 if(running){
  boardBus.poll(now);
  for(unsigned n=0;n<5;++n){unsigned peer=(cursor+n)%5;if(peer<4){if(tires.pollWheel(wired_tire::WheelId(peer+1),tireIncoming))deliveries[peer]=tireIncoming;}else if(brakes.poll(uint64_t(esp_timer_get_time()),brakeIncoming))ui.ingest(brakeIncoming,uint64_t(esp_timer_get_time()));}
  cursor=(cursor+1)%5;ui.commandResult(brakes.pendingResult());
  auto action=ui.touch(touchPanel.poll(now),uint64_t(esp_timer_get_time()));
  if(action.kind==wired_brake::UiActionKind::Apply||action.kind==wired_brake::UiActionKind::Mute){const auto result=brakes.submit(action.intent);if(result==SubmitResult::Accepted)ui.commandResult(brakes.pendingResult());else ui.rejectSubmit();Serial.printf("UI command submit=%u\n",unsigned(result));}
  renderer.poll(uint64_t(esp_timer_get_time()),deliveries,display_config::colorScale,panel,ui);
  if(now>=nextLog){nextLog=wired_brake::deadlineAfter(now,1000000);Serial.printf("UI submitted=%lu refresh=%lu brakeFresh=%u command=%u; FPS requires interval measurement\n",static_cast<unsigned long>(renderer.submitted()),static_cast<unsigned long>(panel.refreshes()),ui.fresh(now),unsigned(ui.result()));}
 }
 yield();
}
