#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <atomic>
#include "BrakeChannelConfig.h"
class BrakeBle:public BLEServerCallbacks,public BLECharacteristicCallbacks {
 public:
  WT_OFFLINE_LINK bool begin(const BrakeHardwareConfig&);
  WT_OFFLINE_LINK void poll(uint64_t,const wired_brake::BrakeTelemetry&);
  void onConnect(BLEServer*)override;
  void onDisconnect(BLEServer*)override;
  void onWrite(BLECharacteristic*)override;
  bool enabled()const{return enabled_;}bool connected()const{return connected_.load();}
  uint32_t attempts()const{return attempts_;}
 private:
  struct FilterCommand{std::array<uint8_t,20>bytes{};size_t size=0;};
  std::array<FilterCommand,16>queue_{};std::atomic<unsigned>write_{0},read_{0};
  std::atomic<bool>connected_{false},restart_{false},reset_{false};
  wired_brake::BrakeCanFilter filter_{};BLECharacteristic*data_=nullptr;bool enabled_=false;unsigned nextPid_=0;uint64_t nextSend_=0;uint32_t attempts_=0;
};
