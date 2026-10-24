#pragma once
#include <cstdint>
namespace test_hw{inline int64_t clockUs=0;}
inline int64_t esp_timer_get_time(){return test_hw::clockUs;}
