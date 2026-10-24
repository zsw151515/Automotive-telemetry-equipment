#pragma once
#include <WiredTireCore.h>
#include "PanelDriver.h"
#include "BoardSpiBus.h"
#ifndef WIRED_TIRE_OFFLINE_BUILD
#define WIRED_TIRE_OFFLINE_BUILD 1
#endif
#if WIRED_TIRE_OFFLINE_BUILD
namespace display_config {
constexpr bool hardwareEnabled = false;
const BoardBusConfig sixPortBus{}; // Task 10 supplies one reviewed five-peer board profile.
constexpr int sck = -1, mosi = -1, miso = -1;
constexpr std::array<int, 4> selects{-1, -1, -1, -1}, ready{-1, -1, -1, -1};
const PanelConfig panel{};
constexpr wired_tire::ColorScale colorScale{};
}
#else
#include "ApprovedDisplayProfile.h"
#endif
