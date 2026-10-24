#pragma once
#include <WiredTireCore.h>
struct BrakeProbePins {int rx=-1,tx=-1,direction=-1;uint8_t address=1;bool enabled=true;};
struct BrakeHardwareConfig {
 bool hardwareEnabled=false,approved=false;
 std::array<BrakeProbePins,2> probes{{{-1,-1,-1,1,true},{-1,-1,-1,2,true}}};
 int sck=-1,mosi=-1,miso=-1,select=-1,ready=-1,buzzerPin=-1;
};
inline bool validBrakeHardware(const BrakeHardwareConfig&c){
 if(!c.hardwareEnabled||!c.approved)return false;
 const int pins[]{c.probes[0].rx,c.probes[0].tx,c.probes[0].direction,c.probes[1].rx,c.probes[1].tx,c.probes[1].direction,c.sck,c.mosi,c.miso,c.select,c.ready,c.buzzerPin};
 if(!wired_tire::distinctPins(pins,12))return false;
 for(int p:pins)if(p>21||(p>=12&&p<=19))return false; // C3 flash/USB reserved; actual module still needs review.
 for(const auto&p:c.probes)if(p.address<1||p.address>247)return false;
 return true;
}
#ifndef WIRED_TIRE_OFFLINE_BUILD
#define WIRED_TIRE_OFFLINE_BUILD 1
#endif
#if WIRED_TIRE_OFFLINE_BUILD
namespace brake_config {const BrakeHardwareConfig hardware{};}
#else
#include "ApprovedBrakeProfile.h"
#endif
