#pragma once
constexpr int UART_NUM_1 = 1, UART_PIN_NO_CHANGE = -1, UART_MODE_RS485_HALF_DUPLEX = 1, ESP_OK = 0;
inline int uart_set_pin(int, int, int, int, int) { return ESP_OK; }
inline int uart_set_mode(int, int) { return ESP_OK; }
