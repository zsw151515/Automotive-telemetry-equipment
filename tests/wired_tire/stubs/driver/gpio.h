#pragma once
#include <array>
using gpio_num_t=int;
namespace test_hw{inline std::array<int,64> levels{};}
inline int gpio_set_level(gpio_num_t p,int v){test_hw::levels.at(p)=v;return 0;}
inline int gpio_get_level(gpio_num_t p){return test_hw::levels.at(p);}
#define GPIO_IS_VALID_GPIO(p) ((p)>=0&&(p)<64)
#define GPIO_IS_VALID_OUTPUT_GPIO(p) GPIO_IS_VALID_GPIO(p)
