#include "BoardSpiBus.h"
#include <driver/gpio.h>
#include <esp_heap_caps.h>
#include <cstring>
BoardSpiBus& defaultBoardSpiBus(){static BoardSpiBus bus;return bus;}
void BoardSpiBus::cleanup(){for(auto&p:peers_){if(p.device)spi_bus_remove_device(p.device);heap_caps_free(p.tx);heap_caps_free(p.rx);p={};}}
bool BoardSpiBus::begin(const BoardBusConfig&c){
 if(enabled_||!c.hardwareEnabled||c.peerCount<1||c.peerCount>5)return false;
 int pins[13]{c.sck,c.mosi,c.miso};for(unsigned i=0;i<c.peerCount;++i){pins[3+i*2]=c.selects[i];pins[4+i*2]=c.ready[i];}
 if(!wired_tire::distinctPins(pins,3+c.peerCount*2))return false;
 for(unsigned i=0;i<3+c.peerCount*2;++i)if(!GPIO_IS_VALID_GPIO(pins[i]))return false;
 spi_bus_config_t bus{};bus.sclk_io_num=c.sck;bus.mosi_io_num=c.mosi;bus.miso_io_num=c.miso;bus.quadwp_io_num=bus.quadhd_io_num=-1;bus.data4_io_num=bus.data5_io_num=bus.data6_io_num=bus.data7_io_num=-1;bus.max_transfer_sz=1728;
 if(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO)!=ESP_OK)return false;
 for(unsigned i=0;i<c.peerCount;++i){auto&p=peers_[i];p.ready=c.ready[i];pinMode(p.ready,INPUT);p.tx=static_cast<uint8_t*>(heap_caps_aligned_alloc(16,1728,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));p.rx=static_cast<uint8_t*>(heap_caps_aligned_alloc(16,1728,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));
  spi_device_interface_config_t device{};device.mode=0;device.clock_speed_hz=1000000;device.spics_io_num=c.selects[i];device.queue_size=1;
  if(!p.tx||!p.rx||spi_bus_add_device(SPI2_HOST,&device,&p.device)!=ESP_OK){cleanup();spi_bus_free(SPI2_HOST);return false;}
 }
 enabled_=true;Serial.println("Shared SPI owner initialized: mode0 1MHz, 1728 tire / 256 brake");return true;
}
void BoardSpiBus::poll(uint64_t now){
 now_=now;if(!enabled_||active_<0)return;
 if(now<started_||now-started_>=100000)late_=true;
 auto&p=peers_[size_t(active_)];spi_transaction_t*done=nullptr;
 if(spi_device_get_trans_result(p.device,&done,0)!=ESP_OK)return;
 p.result={done==&p.transaction&&!late_,late_,p.transaction.length/8,now>=started_?now-started_:UINT64_MAX,now};
 if(p.result.success)std::memcpy(p.destination,p.rx,p.result.length);
 p.completed=true;p.destination=nullptr;active_=-1;
}
bool BoardSpiBus::tryQueue(BoardPeer peer,const uint8_t*tx,uint8_t*rx,size_t n){
 const auto i=unsigned(peer);if(!enabled_||active_>=0||i>=5||!tx||!rx||n!=(i==4?256:1728))return false;
 auto&p=peers_[i];if(!p.device||p.completed||gpio_get_level(gpio_num_t(p.ready))!=HIGH)return false;
 std::memcpy(p.tx,tx,n);std::memset(p.rx,0,n);p.transaction={};p.transaction.length=n*8;p.transaction.tx_buffer=p.tx;p.transaction.rx_buffer=p.rx;
 if(spi_device_queue_trans(p.device,&p.transaction,0)!=ESP_OK)return false;
 p.destination=rx;active_=int(i);started_=now_;late_=false;return true;
}
bool BoardSpiBus::takeCompleted(BoardPeer peer,BusResult&out){const auto i=unsigned(peer);if(i>=5||!peers_[i].completed)return false;out=peers_[i].result;peers_[i].completed=false;return true;}
