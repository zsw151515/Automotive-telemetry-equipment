#include "RaceChronoBle.h"
#include <esp_mac.h>
using namespace wired_tire;
bool RaceChronoBle::begin(WheelId wheel, const LegacyAuxFields& aux, bool auxiliaryFieldsVerified) {
  if (enabled_ || !validWheel(wheel) || !auxiliaryFieldsVerified) {
    Serial.println("BLE not started: auxiliary-field compatibility must be verified");
    return false;
  }
  uint8_t mac[6]{}; char name[32]{};
  if (esp_read_mac(mac, ESP_MAC_BT) != ESP_OK || !rejsaName(wheel, mac, name, sizeof(name))) return false;
  aux_ = aux;
  BLEDevice::init(name);
  BLEDevice::setPower(ESP_PWR_LVL_P9); // Existing v3 reference; four-radio enclosure still needs testing.
  auto* server = BLEDevice::createServer();
  server->setCallbacks(this);
  auto* service = server->createService(BLEUUID(uint16_t{0x1FF7}));
  for (size_t i = 0; i < 3; ++i) {
    chars_[i] = service->createCharacteristic(BLEUUID(static_cast<uint16_t>(i + 1)),
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    chars_[i]->addDescriptor(new BLE2902());
    uint8_t notReady[20]{}; // Not a valid 0x02 temperature notification.
    chars_[i]->setValue(notReady, sizeof(notReady));
  }
  service->start();
  auto* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLEUUID(uint16_t{0x1FF7}));
  advertising->setScanResponse(true);
  BLEDevice::startAdvertising();
  enabled_ = true;
  Serial.printf("BLE advertising started: %s, RejsaRubberTrac (not DIY CAN)\n", name);
  return true;
}
void RaceChronoBle::onConnect(BLEServer*) {
  generation_.fetch_add(1);
  connected_.store(true);
  Serial.println("BLE connected");
}
void RaceChronoBle::onDisconnect(BLEServer*) {
  connected_.store(false); restart_.store(true);
  Serial.println("BLE disconnected");
}
void RaceChronoBle::poll(uint64_t nowUs, const FrameMeta& meta, const ZoneResult& zones) {
  if (!enabled_) return;
  if (restart_.exchange(false) && !connected_.load()) {
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising restarted");
  }
  if (!connected_.load()) { part_ = 0; return; }
  const uint32_t generation = generation_.load();
  if (generation != seenGeneration_) {
    seenGeneration_ = generation; connectedAtUs_ = nowUs; nextBatchUs_ = nowUs; part_ = 0;
  }
  if (nowUs - connectedAtUs_ < 800000 || !canNotify(meta, zones, nowUs, true)) { part_ = 0; return; }
  if (part_ == 0) {
    if (nowUs < nextBatchUs_) return;
    batchMeta_ = meta; batchZones_ = zones;
    nextBatchUs_ = nowUs + 30000; nextPartUs_ = nowUs;
  }
  if (nowUs < nextPartUs_ || !canNotify(batchMeta_, batchZones_, nowUs, true)) return;
  uint8_t packet[20];
  if (!buildRaceChronoPacket(static_cast<uint8_t>(part_ + 1), batchZones_, aux_, packet)) { part_ = 0; return; }
  chars_[part_]->setValue(packet, sizeof(packet));
  chars_[part_]->notify();
  ++notificationAttempts_;
  part_ = static_cast<uint8_t>((part_ + 1) % 3);
  nextPartUs_ = nowUs + 1000;
}
