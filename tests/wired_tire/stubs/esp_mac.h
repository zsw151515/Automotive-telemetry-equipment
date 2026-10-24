#pragma once
#include <cstdint>
#include "esp_err.h"
constexpr int ESP_MAC_BT = 0;
inline int esp_read_mac(uint8_t* bytes, int) { for (unsigned i = 0; i < 6; ++i) bytes[i] = static_cast<uint8_t>(i); return ESP_OK; }
