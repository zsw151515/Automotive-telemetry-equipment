#pragma once
#include "BrakeTypes.h"
#include <cstddef>
namespace wired_brake {
size_t encodeBrakeCan(uint32_t,const BrakeTelemetry&,uint8_t out[20]);
class BrakeCanFilter {
 public:bool apply(const uint8_t*,size_t);bool due(uint32_t,uint64_t now);
 private:
  struct Entry{uint32_t pid=0;uint16_t intervalMs=150;uint64_t last=0;bool used=false,sent=false;};
  std::array<Entry,12> entries_{};std::array<Entry,8> allTimers_{};bool allowAll_=true;uint16_t allInterval_=150;
};
}
