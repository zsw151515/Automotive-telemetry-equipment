#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <WiredTireCore.h>
#include <atomic>
class RaceChronoBle : public BLEServerCallbacks {
 public:
  WT_OFFLINE_LINK bool begin(wired_tire::WheelId, const wired_tire::LegacyAuxFields&, bool auxiliaryFieldsVerified);
  WT_OFFLINE_LINK void poll(uint64_t nowUs, const wired_tire::FrameMeta&, const wired_tire::ZoneResult&);
  bool connected() const { return connected_.load(); }
  bool enabled() const { return enabled_; }
  uint32_t notificationAttempts() const { return notificationAttempts_; }
  void onConnect(BLEServer*) override;
  void onDisconnect(BLEServer*) override;
 private:
  BLECharacteristic* chars_[3]{};
  wired_tire::LegacyAuxFields aux_;
  wired_tire::FrameMeta batchMeta_;
  wired_tire::ZoneResult batchZones_;
  std::atomic<bool> connected_{false}, restart_{false};
  std::atomic<uint32_t> generation_{0};
  uint32_t seenGeneration_ = 0;
  uint64_t connectedAtUs_ = 0, nextBatchUs_ = 0, nextPartUs_ = 0;
  uint8_t part_ = 0;
  bool enabled_ = false;
  uint32_t notificationAttempts_ = 0;
};
