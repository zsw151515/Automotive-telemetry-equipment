#pragma once
#include <WiredTireCore.h>
#include "Board7BProfile.h"
#ifndef WIRED_TIRE_OFFLINE_BUILD
#define WIRED_TIRE_OFFLINE_BUILD 1
#endif
#if WIRED_TIRE_OFFLINE_BUILD
namespace display_config {const Board7BProfile board{};constexpr wired_tire::ColorScale colorScale{};}
#else
#include "ApprovedDisplayProfile.h"
#endif
