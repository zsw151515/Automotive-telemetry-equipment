#pragma once
#include "esp_err.h"
#include <cstddef>
constexpr int SPI2_HOST=1,SPI_DMA_CH_AUTO=0;
struct spi_bus_config_t{int mosi_io_num=0,miso_io_num=0,sclk_io_num=0,quadwp_io_num=0,quadhd_io_num=0,data4_io_num=0,data5_io_num=0,data6_io_num=0,data7_io_num=0,max_transfer_sz=0;};
struct spi_slave_transaction_t{size_t length=0,trans_len=0;const void*tx_buffer=nullptr;void*rx_buffer=nullptr;void*user=nullptr;};
struct spi_slave_interface_config_t{int spics_io_num=0,queue_size=0,mode=0;void(*post_setup_cb)(spi_slave_transaction_t*)=nullptr;void(*post_trans_cb)(spi_slave_transaction_t*)=nullptr;};
namespace test_hw{inline spi_slave_transaction_t*slaveTransaction=nullptr;inline bool slaveDone=false;inline unsigned slaveInitializations=0;inline spi_slave_interface_config_t slaveConfig{};}
inline int spi_slave_initialize(int,const spi_bus_config_t*,const spi_slave_interface_config_t*c,int){++test_hw::slaveInitializations;test_hw::slaveConfig=*c;return ESP_OK;}
inline int spi_slave_queue_trans(int,spi_slave_transaction_t*t,int){test_hw::slaveTransaction=t;if(test_hw::slaveConfig.post_setup_cb)test_hw::slaveConfig.post_setup_cb(t);return ESP_OK;}
inline int spi_slave_get_trans_result(int,spi_slave_transaction_t**t,int){if(!test_hw::slaveDone)return ESP_ERR_TIMEOUT;*t=test_hw::slaveTransaction;test_hw::slaveDone=false;if(test_hw::slaveConfig.post_trans_cb)test_hw::slaveConfig.post_trans_cb(*t);return ESP_OK;}
