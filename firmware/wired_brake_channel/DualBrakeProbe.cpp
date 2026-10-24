#include "DualBrakeProbe.h"
#include <driver/uart.h>
using namespace wired_brake;
bool DualBrakeProbe::begin(const BrakeHardwareConfig&c){
 if(enabled_||!validBrakeHardware(c))return false;
 cfg_=c;HardwareSerial*ports[]{&port0_,&port1_};
 for(unsigned i=0;i<2;++i){const auto&p=c.probes[i];if(!p.enabled)continue;
  pinMode(p.direction,OUTPUT);digitalWrite(p.direction,LOW);
  ports[i]->begin(9600,SERIAL_8N1,p.rx,p.tx);
  if(uart_set_pin(uart_port_t(i),UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE,p.direction,UART_PIN_NO_CHANGE)!=ESP_OK||uart_set_mode(uart_port_t(i),UART_MODE_RS485_HALF_DUPLEX)!=ESP_OK){port0_.end();port1_.end();return false;}
  mask_|=1<<i;Serial.printf("Brake probe %u UART%u 9600 8N1 addr=%u RX=%d TX=%d DE=%d read-only\n",i,i,p.address,p.rx,p.tx,p.direction);
 }
 enabled_=true;return true;
}
void DualBrakeProbe::poll(uint64_t now){
 if(!enabled_)return;
 if(!clockStarted_||now<start_){clockStarted_=true;start_=now;}
 HardwareSerial*ports[]{&port0_,&port1_};
 for(unsigned i=0;i<2;++i){if(!(mask_&(1<<i))||now-start_<i*100000)continue;
  auto&port=*ports[i];auto&poller=pollers_[i];const auto action=poller.poll(now);
  if(action==PollAction::Timeout){receiving_[i]=false;count_[i]=0;++stats_.timeouts;}
  size_t budget=32;
  while(port.available()&&budget--){const int byte=port.read();if(byte<0)break;
   if(!receiving_[i]){++stats_.dropped;continue;}
   if(count_[i]>=16){receiving_[i]=false;poller.response(now,false);++stats_.lengthErrors;break;}
   bytes_[i][count_[i]++]=uint8_t(byte);
   const auto n=count_[i];if(n<2)continue;
   const size_t expected=(bytes_[i][1]&0x80)?5:7;
   if(n==expected){int16_t temperature=0;const bool parsed=decodeBrakeRegister(bytes_[i],n,cfg_.probes[i].address,poller.function(),temperature);
    receiving_[i]=false;
    if(poller.response(now,parsed)){const bool old=readings_[i].valid;if(old&&elapsedMs(now,readings_[i].receivedUs)>2500)++stats_.recoveries;readings_[i]={temperature,true,now};if(!old)Serial.printf("Brake probe %u initialized / first valid reading\n",i);}
    else{const auto crc=wired_tire::modbusCrc(bytes_[i],n-2);if(bytes_[i][n-2]!=uint8_t(crc)||bytes_[i][n-1]!=uint8_t(crc>>8))++stats_.crcErrors;else ++stats_.lengthErrors;}
    break;
   }
  }
  if(action==PollAction::Request){receiving_[i]=false;count_[i]=0;
   if(port.available()){poller.response(now,false);++stats_.dropped;continue;}
   uint8_t request[8];encodeBrakeRead(cfg_.probes[i].address,poller.function(),poller.reg(),request);
   if(port.write(request,8)==8)receiving_[i]=true;else{poller.response(now,false);++stats_.dropped;}
  }
 }
}
