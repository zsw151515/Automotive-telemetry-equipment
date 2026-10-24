#pragma once
#include "BrakeWire.h"
#include "ThermalView.h"
namespace wired_brake {
enum class Page{Tire,Brake,Settings};
struct TouchSample{uint16_t x=0,y=0;uint8_t count=0;bool pressed=false,valid=false;uint64_t sampledUs=0;};
enum class UiActionKind{None,Navigate,DraftChanged,Apply,Mute};
struct UiAction{UiActionKind kind=UiActionKind::None;ControlIntent intent{};};
class TelemetryUi {
 public:
  Page page()const{return page_;}
  void ingest(const BrakeDelivery&,uint64_t);
  UiAction touch(const TouchSample&,uint64_t);
  void render(const std::array<wired_tire::SnapshotDelivery,4>&,uint64_t,const wired_tire::ColorScale&,wired_tire::PixelSurface&,bool simulation=false);
  const BrakeConfig& draft()const{return draft_;}const BrakeConfig& effective()const{return latest_.state.config;}
  bool fresh(uint64_t now)const{return updateBrakeView(latest_,now).stateFresh;}
  bool pending()const{return pending_;}AckResult result()const{return result_;}
  void commandResult(const BrakeAck&);
  void rejectSubmit(){pending_=false;result_=AckResult::Invalid;tracked_={};}
 private:
  Page page_=Page::Tire;BrakeDelivery latest_{};BrakeConfig draft_{};BrakeAck tracked_{};AckResult result_=AckResult::None;
  bool have_=false,editing_=false,pending_=false,pressed_=false,blocked_=false,fired_=false,clockSet_=false;
  int target_=-1;uint64_t pressedAt_=0,nextRepeat_=0,lastClock_=0;
  int hit(uint16_t,uint16_t)const;
  void cancel(){pressed_=false;blocked_=true;fired_=false;target_=-1;}
  UiAction activate(int);
};
}
