#include "RasterPrimitives.h"
#include <algorithm>
namespace wired_tire::raster {
bool validSurface(const PixelSurface& surface) {
  if (!surface.pixels || !surface.width || !surface.height || surface.stride < surface.width ||
      surface.capacity < surface.width || surface.width > SIZE_MAX / kCols || surface.height > SIZE_MAX / kRows) return false;
  return surface.height - 1 <= (surface.capacity - surface.width) / surface.stride;
}
std::array<uint8_t,7> glyph(char value) {
  switch (value) {
    case 'A': return {14,17,17,31,17,17,17};
    case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14};
    case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31};
    case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15};
    case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14};
    case 'J': return {7,2,2,2,2,18,12};
    case 'K': return {17,18,20,24,20,18,17};
    case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17};
    case 'N': return {17,25,25,21,19,19,17};
    case 'O': return {14,17,17,17,17,17,14};
    case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13};
    case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30};
    case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14};
    case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10};
    case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4};
    case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14};
    case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31};
    case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2};
    case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14};
    case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14};
    case '9': return {14,17,17,15,1,1,14};
    case '.': return {0,0,0,0,0,6,6};
    case '+': return {0,4,4,31,4,4,0};
    case '/': return {1,2,2,4,8,8,16};
    case '-': return {0,0,0,31,0,0,0};
    case ':': return {0,6,6,0,6,6,0};
    default: return {};
  }
}
void fill(PixelSurface& surface, size_t left, size_t top, size_t width, size_t height, uint16_t color) {
  if (!validSurface(surface) || left >= surface.width || top >= surface.height) return;
  width = std::min(width, surface.width - left); height = std::min(height, surface.height - top);
  for (size_t y = 0; y < height; ++y)
    std::fill_n(surface.pixels + (top + y) * surface.stride + left, width, color);
}
void text(PixelSurface& surface, size_t left, size_t top, const char* value, unsigned scale, uint16_t color) {
  if (!value || !scale || !validSurface(surface) || left >= surface.width || top >= surface.height || scale > 1024) return;
  for (size_t letter = 0; value[letter] && letter < 64; ++letter) {
    const auto rows = glyph(value[letter]);
    for (size_t y = 0; y < 7; ++y)
      for (size_t x = 0; x < 5; ++x)
        if (rows[y] & (1U << (4 - x))) fill(surface, left + (letter * 6 + x) * scale, top + y * scale, scale, scale, color);
  }
}
}
