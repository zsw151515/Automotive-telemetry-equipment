#pragma once
#include "BLE2902.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
constexpr int ESP_PWR_LVL_P9 = 9;
class BLEServer;
class BLEServerCallbacks {
 public:
  virtual ~BLEServerCallbacks() = default;
  virtual void onConnect(BLEServer*) {}
  virtual void onDisconnect(BLEServer*) {}
};
struct BLEUUID { explicit BLEUUID(uint16_t v) : value(v) {} uint16_t value; };
inline std::vector<uint16_t> testNotifications;
class BLECharacteristic {
 public:
  static constexpr int PROPERTY_READ = 1, PROPERTY_NOTIFY = 2;
  uint16_t id = 0;
  void addDescriptor(BLE2902* descriptor) { delete descriptor; }
  void setValue(const uint8_t*, size_t) {}
  void notify() { testNotifications.push_back(id); }
};
class BLEService {
 public:
  std::array<BLECharacteristic,3> chars;
  BLECharacteristic* createCharacteristic(BLEUUID uuid, int) {
    auto& c = chars.at(uuid.value - 1); c.id = uuid.value; return &c;
  }
  void start() {}
};
class BLEServer {
 public:
  BLEService service;
  void setCallbacks(BLEServerCallbacks*) {}
  BLEService* createService(BLEUUID) { return &service; }
};
class BLEAdvertising {
 public:
  void addServiceUUID(BLEUUID) {}
  void setScanResponse(bool) {}
};
class BLEDevice {
 public:
  static void init(const char*) {}
  static void setPower(int) {}
  static BLEServer* createServer() { static BLEServer server; return &server; }
  static BLEAdvertising* getAdvertising() { static BLEAdvertising advertising; return &advertising; }
  static void startAdvertising() {}
};
