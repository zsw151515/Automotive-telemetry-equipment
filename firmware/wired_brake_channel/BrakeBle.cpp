#include "BrakeBle.h"
#include <cstring>
using namespace wired_brake;
bool BrakeBle::begin(const BrakeHardwareConfig&c){
 if(enabled_||!validBrakeHardware(c))return false;
 BLEDevice::init("N BrakeTemp");auto*server=BLEDevice::createServer();server->setCallbacks(this);
 auto*service=server->createService(BLEUUID(uint16_t(0x1ff8)));
 data_=service->createCharacteristic(BLEUUID(uint16_t(1)),BLECharacteristic::PROPERTY_READ|BLECharacteristic::PROPERTY_NOTIFY);data_->addDescriptor(new BLE2902());
 auto*filter=service->createCharacteristic(BLEUUID(uint16_t(2)),BLECharacteristic::PROPERTY_WRITE);filter->setCallbacks(this);
 service->start();auto*ad=BLEDevice::getAdvertising();ad->addServiceUUID(BLEUUID(uint16_t(0x1ff8)));ad->setScanResponse(true);BLEDevice::startAdvertising();enabled_=true;
 Serial.println("BLE advertising started: N BrakeTemp / DIY CAN 1FF8");return true;
}
void BrakeBle::onConnect(BLEServer*){reset_.store(true);connected_.store(true);Serial.println("Brake BLE connected");}
void BrakeBle::onDisconnect(BLEServer*){connected_.store(false);restart_.store(true);Serial.println("Brake BLE disconnected");}
void BrakeBle::onWrite(BLECharacteristic*c){
 auto value=c->getValue();if(value.length()<1||value.length()>20)return;
 const auto w=write_.load(std::memory_order_relaxed);const auto next=(w+1)%queue_.size();if(next==read_.load(std::memory_order_acquire))return;
 auto&cmd=queue_[w];cmd.size=value.length();std::memcpy(cmd.bytes.data(),value.c_str(),cmd.size);write_.store(unsigned(next),std::memory_order_release);
}
void BrakeBle::poll(uint64_t now,const BrakeTelemetry&s){
 if(!enabled_)return;
 if(restart_.exchange(false)&&!connected_.load()){BLEDevice::startAdvertising();Serial.println("Brake BLE advertising restarted");}
 if(reset_.exchange(false)){filter_={};nextPid_=0;nextSend_=now;}
 auto r=read_.load(std::memory_order_relaxed);if(r!=write_.load(std::memory_order_acquire)){const auto cmd=queue_[r];filter_.apply(cmd.bytes.data(),cmd.size);read_.store(unsigned((r+1)%queue_.size()),std::memory_order_release);}
 if(!connected_.load()||now<nextSend_)return;
 nextSend_=deadlineAfter(now,1000);
 for(unsigned n=0;n<8;++n){const uint32_t pid=0x100+nextPid_;nextPid_=(nextPid_+1)%8;if(!filter_.due(pid,now))continue;uint8_t b[20];const auto len=encodeBrakeCan(pid,s,b);data_->setValue(b,len);data_->notify();++attempts_;break;}
}
