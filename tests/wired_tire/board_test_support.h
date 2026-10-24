#pragma once
#include "brake_test_support.h"
#include "BoardSpiBus.h"
#include <driver/gpio.h>
inline BoardBusConfig busProfile(){BoardBusConfig c{};c.hardwareEnabled=true;c.sck=0;c.mosi=1;c.miso=2;c.selects={3,4,5,6,7};c.ready={8,9,10,11,12};for(int p:c.ready)test_hw::levels[p]=1;return c;}
