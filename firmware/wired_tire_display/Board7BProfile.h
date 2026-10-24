#pragma once
#include <cstdint>
#include <Board7BRules.h>
struct Board7BProfile {
 bool hardwareEnabled=false,panelApproved=false,touchApproved=false;
 wired_brake::Board7BPinPlan pins{};
 // Fixed official BSP NC polling policy still requires physical review; never guess reset GPIO23.
 bool touchNcPollingApproved=false;uint8_t brightness=180;
};
inline bool approved7B(const Board7BProfile&p){return p.hardwareEnabled&&p.panelApproved&&p.touchApproved&&p.touchNcPollingApproved&&wired_brake::validateBoardPins(p.pins)==wired_brake::ProfileResult::Ok;}
