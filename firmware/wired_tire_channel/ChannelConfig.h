#pragma once
#include <WiredTireCore.h>
#ifndef WIRED_TIRE_OFFLINE_BUILD
#define WIRED_TIRE_OFFLINE_BUILD 1
#endif
#if WIRED_TIRE_OFFLINE_BUILD
namespace channel_config {
constexpr bool hardwareEnabled = false;
constexpr wired_tire::WheelId wheel = static_cast<wired_tire::WheelId>(0);
constexpr int rx = -1, tx = -1, direction = -1;
constexpr int sck = -1, mosi = -1, miso = -1, select = -1, ready = -1;
constexpr bool bleAuxVerified = false;
const wired_tire::LegacyAuxFields auxiliaryFields{};
}
#else
// This file must be created from a reviewed physical pinmap, never from guessed pins.
#include "ApprovedChannelProfile.h"
#endif
