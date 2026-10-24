#pragma once
#include "ThermalView.h"
namespace wired_tire::raster {
bool validSurface(const PixelSurface&);
void fill(PixelSurface&,size_t,size_t,size_t,size_t,uint16_t);
void text(PixelSurface&,size_t,size_t,const char*,unsigned,uint16_t);
}
