#pragma once
#include "BrakeTypes.h"
#include "BrakeModbus.h"
#if defined(ARDUINO) && (!defined(WIRED_TIRE_OFFLINE_BUILD) || WIRED_TIRE_OFFLINE_BUILD)
// Offline builds retain complete driver entry points for a real SDK link check.
// This attribute never enables IO; runtime profile gates remain authoritative.
#define WT_OFFLINE_LINK __attribute__((used, retain, noipa))
#else
#define WT_OFFLINE_LINK
#endif
#include "ThermalFrame.h"
#include "Yf90640Protocol.h"
#include "TireZones.h"
#include "ProbeSchedule.h"
#include "RaceChronoPacket.h"
#include "SnapshotWire.h"
#include "ThermalView.h"
