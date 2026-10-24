#pragma once
#include "spi_slave.h"
struct TestSpiDevice{int select=-1;};using spi_device_handle_t=TestSpiDevice*;
struct spi_device_interface_config_t{int clock_speed_hz=0,mode=0,spics_io_num=0,queue_size=0;};
struct spi_transaction_t{size_t length=0;const void*tx_buffer=nullptr;void*rx_buffer=nullptr;};
namespace test_hw{inline spi_transaction_t*masterTransaction=nullptr;inline spi_device_handle_t masterDevice=nullptr;inline bool masterDone=false;inline unsigned masterQueues=0;}
inline int spi_bus_initialize(int,const spi_bus_config_t*,int){return ESP_OK;}
inline int spi_bus_free(int){return ESP_OK;}
inline int spi_bus_add_device(int,const spi_device_interface_config_t*c,spi_device_handle_t*d){*d=new TestSpiDevice{c->spics_io_num};return ESP_OK;}
inline int spi_bus_remove_device(spi_device_handle_t d){delete d;return ESP_OK;}
inline int spi_device_queue_trans(spi_device_handle_t d,spi_transaction_t*t,int){test_hw::masterDevice=d;test_hw::masterTransaction=t;++test_hw::masterQueues;return ESP_OK;}
inline int spi_device_get_trans_result(spi_device_handle_t,spi_transaction_t**out,int){if(!test_hw::masterDone)return ESP_ERR_TIMEOUT;*out=test_hw::masterTransaction;test_hw::masterDone=false;return ESP_OK;}
