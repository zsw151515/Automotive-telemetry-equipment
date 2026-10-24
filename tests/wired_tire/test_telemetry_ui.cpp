#include "ui_test_support.h"
#include <fstream>
static std::array<wired_tire::SnapshotDelivery,4> tires(){std::array<wired_tire::SnapshotDelivery,4>a{};for(unsigned w=0;w<4;++w){auto&s=a[w].snapshot;s.meta.wheel=wired_tire::WheelId(w+1);s.meta.bootId=1;s.meta.state=wired_tire::FrameState::Valid;s.ageMs=0;s.zones.mirrored=w%2;for(size_t p=0;p<768;++p){s.frame.temp10[p]=int16_t(200+p);s.frame.setValid(p,p!=300);}}return a;}
int main(int argc,char**argv){
 runTest("four_full_maps_mirror_invalid_pixels_survive_navigation_alarm",[]{TelemetryUi u;CHECK(u.page()==Page::Tire);BrakeDelivery d{};d.state=validBrakeTelemetry();d.state.state=BrakeState::Alarm;u.ingest(d,0);const auto a=tires();std::vector<uint16_t>pixels(1024*600+2,0xabcd);wired_tire::PixelSurface surface{pixels.data()+1,1024,600,1024,1024*600};u.render(a,0,{},surface);
  for(unsigned w=0;w<4;++w)for(unsigned r=0;r<24;++r)for(unsigned c=0;c<32;++c){const unsigned p=r*32+(w%2?31-c:c);const unsigned x=(w%2)*512+12+c*8+4,y=(w/2)*235+130+r*8+4;CHECK(surface.pixels[y*1024+x]==wired_tire::heatColor(a[w].snapshot.frame.temp10[p],p!=300,{}));}
  CHECK(pixels.front()==0xabcd&&pixels.back()==0xabcd);
  uint64_t t=100000;for(auto x:{uint16_t(100),uint16_t(350),uint16_t(650)}){tap(u,x,20,t);u.render(a,t+50000,{},surface);CHECK(surface.pixels[60*1024+2]==0xf800&&pixels.front()==0xabcd&&pixels.back()==0xabcd);t+=100000;}
 });
 runTest("editing_draft_does_not_change_effective_alarm_saved_only_after_matching_ack",[]{TelemetryUi u;uiState(u);tap(u,650,20,0);tap(u,300,320,100000);CHECK(u.draft().alarmC==610&&u.effective().alarmC==600);auto a=tap(u,850,420,200000);CHECK(a.kind==UiActionKind::Apply&&a.intent.candidate.alarmC==610);u.commandResult({AckResult::Pending,CommandOp::ApplySave,7,2,1,1});
  BrakeDelivery d{};d.state=validBrakeTelemetry();d.receivedLocalUs=300000;d.state.ack={AckResult::AppliedSaved,CommandOp::ApplySave,7,1,1,2};u.ingest(d,300000);CHECK(u.result()!=AckResult::AppliedSaved&&u.effective().alarmC==600);
  d.state.config.alarmC=610;d.state.configVersion=d.state.savedVersion=2;d.state.ack.commandId=2;u.ingest(d,300000);CHECK(u.result()==AckResult::AppliedSaved&&u.effective().alarmC==610);
 });
 runTest("unknown_readback_and_new_brake_boot_drops_draft_write",[]{TelemetryUi u;uiState(u);tap(u,650,20,0);tap(u,300,320,100000);tap(u,850,420,200000);u.commandResult({AckResult::Pending,CommandOp::ApplySave,7,2,1,1});u.commandResult({AckResult::OutcomeUnknown,CommandOp::ApplySave,7,2,1,1});CHECK(u.result()==AckResult::OutcomeUnknown);
  BrakeDelivery d{};d.state=validBrakeTelemetry();d.state.config.alarmC=610;d.state.configVersion=d.state.savedVersion=2;d.receivedLocalUs=300000;u.ingest(d,300000);CHECK(u.effective().alarmC==610);
  d.state.brakeBootId=2;d.state.config.alarmC=650;u.ingest(d,300000);CHECK(u.draft().alarmC==650&&!u.pending());CHECK(!u.fresh(2000000));
 });
 if(argc==3&&std::string(argv[1])=="--preview"){
  TelemetryUi u;uiState(u);auto a=tires();std::vector<uint16_t>p(1024*600);wired_tire::PixelSurface s{p.data(),1024,600,1024,p.size()};
  for(unsigned page=0;page<3;++page){tap(u,uint16_t(100+page*250),20,page*100000);u.render(a,page*100000+50000,{},s,true);std::ofstream f(std::string(argv[2])+"-"+std::to_string(page)+".ppm",std::ios::binary);f<<"P6\n1024 600\n255\n";for(auto v:p){char rgb[]{char(((v>>11)&31)*255/31),char(((v>>5)&63)*255/63),char((v&31)*255/31)};f.write(rgb,3);}CHECK(f.good());}
 }
}
