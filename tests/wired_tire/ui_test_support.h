#pragma once
#include "brake_test_support.h"
#include "TelemetryUi.h"
inline TouchSample down(uint16_t x,uint16_t y,uint64_t t){return {x,y,1,true,true,t};}
inline void release(TelemetryUi&u,uint64_t t){u.touch({0,0,0,false,true,t},t);}
inline UiAction tap(TelemetryUi&u,uint16_t x,uint16_t y,uint64_t t){release(u,t);u.touch(down(x,y,t),t);return u.touch(down(x,y,t+50000),t+50000);}
inline void uiState(TelemetryUi&u,uint64_t now=0){BrakeDelivery d{};d.state=validBrakeTelemetry();d.receivedLocalUs=now;u.ingest(d,now);}
