#include "BrakeSpiSlave.h"
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <cstring>
using namespace wired_brake;
void BrakeSpiSlave::prepared(spi_slave_transaction_t*t){auto*s=static_cast<BrakeSpiSlave*>(t->user);gpio_set_level(gpio_num_t(s->ready_),1);}
void BrakeSpiSlave::completed(spi_slave_transaction_t*t){auto*s=static_cast<BrakeSpiSlave*>(t->user);gpio_set_level(gpio_num_t(s->ready_),0);}
bool BrakeSpiSlave::queue(){
 if(queued_)return true;
 std::memset(rx_,0,256);transaction_={};transaction_.length=2048;transaction_.tx_buffer=tx_;transaction_.rx_buffer=rx_;transaction_.user=this;
 if(spi_slave_queue_trans(SPI2_HOST,&transaction_,0)!=ESP_OK)return false;
 queued_=true;return true;
}
bool BrakeSpiSlave::begin(const BrakeHardwareConfig&c){
 if(enabled_||!validBrakeHardware(c))return false;
 ready_=c.ready;pinMode(ready_,OUTPUT);digitalWrite(ready_,LOW);
 tx_=static_cast<uint8_t*>(heap_caps_aligned_alloc(16,256,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));rx_=static_cast<uint8_t*>(heap_caps_aligned_alloc(16,256,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));
 if(!tx_||!rx_){heap_caps_free(tx_);heap_caps_free(rx_);return false;}
 std::memset(tx_,0,256);spi_bus_config_t bus{};bus.sclk_io_num=c.sck;bus.mosi_io_num=c.mosi;bus.miso_io_num=c.miso;bus.quadwp_io_num=bus.quadhd_io_num=-1;bus.data4_io_num=bus.data5_io_num=bus.data6_io_num=bus.data7_io_num=-1;bus.max_transfer_sz=256;
 spi_slave_interface_config_t slave{};slave.spics_io_num=c.select;slave.queue_size=1;slave.mode=0;slave.post_setup_cb=prepared;slave.post_trans_cb=completed;
 if(spi_slave_initialize(SPI2_HOST,&bus,&slave,SPI_DMA_CH_AUTO)!=ESP_OK){heap_caps_free(tx_);heap_caps_free(rx_);return false;}
 enabled_=true;Serial.println("Brake SPI slave initialized: mode0 / 256 bytes / independent CS READY");return true;
}
void BrakeSpiSlave::poll(uint64_t now){
 if(!enabled_)return;
 if(!queued_){queue();return;}
 spi_slave_transaction_t*done=nullptr;if(spi_slave_get_trans_result(SPI2_HOST,&done,0)!=ESP_OK)return;
 queued_=false;BrakeCommand c{};uint32_t token=0;
 if(done==&transaction_&&done->trans_len==2048&&decodeBrakeRequest(rx_,256,c,token)){
  if(!hasCommand_){command_=c;hasCommand_=true;}
  auto s=state_;s.stateAgeMs=elapsedMs(now,s.sourceStateUs);
  const uint32_t extra=elapsedMs(now,state_.sourceStateUs);
  for(unsigned i=0;i<2;++i){const uint64_t age=uint64_t(s.sensorAgeMs[i])+extra;s.sensorAgeMs[i]=age>UINT32_MAX?UINT32_MAX:uint32_t(age);}
  encodeBrakeStatus(s,token,tx_,256);
 }
 queue();
}
bool BrakeSpiSlave::takeCommand(BrakeCommand&out){if(!hasCommand_)return false;out=command_;hasCommand_=false;return true;}
