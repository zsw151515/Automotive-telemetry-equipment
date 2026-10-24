#include "ui_test_support.h"
int main(){
 runTest("held_multitouch_stale_touch_cannot_repeat_save_or_mute",[]{TelemetryUi u;uiState(u);CHECK(u.touch({10,10,2,true,true,0},0).kind==UiActionKind::None);release(u,0);tap(u,650,20,0);
  CHECK(u.touch(down(850,420,50001),50001).kind==UiActionKind::None);release(u,100000);auto a=tap(u,850,420,150000);CHECK(a.kind==UiActionKind::Apply);
  for(uint64_t t=210000;t<900000;t+=10000)CHECK(u.touch(down(850,420,t),t).kind==UiActionKind::None);
  u.rejectSubmit();release(u,900000);u.touch(down(800,520,900000),900000);
  CHECK(u.touch(down(800,520,900000),1900000).kind==UiActionKind::None);release(u,1900001);
  unsigned count=0;for(uint64_t t=2000000;t<=4000000;t+=50000){uiState(u,t);if(u.touch(down(800,520,t),t).kind==UiActionKind::Mute)++count;}CHECK(count==1);
 });
 runTest("repeat_only_draft_bounds_and_clock_regression_cancel",[]{TelemetryUi u;uiState(u);tap(u,650,20,0);release(u,100000);unsigned edits=0;
  for(uint64_t t=100000;t<=1200000;t+=50000){uiState(u,t);if(u.touch(down(300,320,t),t).kind==UiActionKind::DraftChanged)++edits;}
  CHECK(edits==5&&u.draft().alarmC==650&&u.effective().alarmC==600);CHECK(u.touch(down(850,420,0),0).kind==UiActionKind::None);CHECK(u.touch({1024,420,1,true,true,0},0).kind==UiActionKind::None);
 });
}
