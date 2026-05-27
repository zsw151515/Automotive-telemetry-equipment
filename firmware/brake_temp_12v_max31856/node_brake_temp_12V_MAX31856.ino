/*
  Brake Temperature Node 12V MAX31856 v3
  ESP32-C3 + MAX31856 + RaceChrono BLE DIY CAN

  Hardware: brake_temp_12V_MAX31856 2026-05-22
  MCU: ESP32-C3 module
  Thermocouple IC: MAX31856MUD+
  Thermocouple type: K-type
  Power: 12V -> AP63203 -> SYS_3V3, MAX31856 via TC_3V3 filter

  Pin map from schematic:
    MAX_MOSI  = GPIO6
    MAX_MISO  = GPIO7
    MAX_SCK   = GPIO8
    MAX_CS    = GPIO10
    LED_STAT  = GPIO2
    BTN_SENSE = GPIO3, currently unused

  RaceChrono BLE DIY CAN:
    Service UUID: 0x1FF8
    CAN main char: 0x0001
    CAN filter char: 0x0002

  Payload of PID_BRAKE_TEMP:
    byte0-1: temperature x10, unsigned 16-bit, big-endian
             example: 256 = 25.6C
    byte2  : MAX31856 fault raw
    byte3  : status bits
             bit0 = hardware reading valid now
             bit1 = using held last valid temperature
             bit2 = no valid temperature yet
    byte4-5: cold junction x10, signed int16, big-endian
    byte6-7: sample counter, unsigned 16-bit, big-endian

  RaceChrono formula for temperature:
    ((B0 * 256) + B1) / 10

  Notes:
    1. This code does NOT immediately send 0C on transient MAX31856 fault.
       It holds the last valid value for FAULT_HOLD_MS.
    2. If fault lasts longer than FAULT_HOLD_MS, it sends 0C and LED enters fault blink.
    3. For Taiwan / 60Hz area, default filter is 60Hz.
       For mainland China, you may switch USE_50HZ_FILTER to true.
*/

#include <Arduino.h>
#include <math.h>
#include <Adafruit_MAX31856.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ============================================================
// User configuration
// ============================================================

// ----- Change these when flashing another wheel/node -----
#define BLE_DEVICE_NAME       "BrakeTC_FL"
#define PID_BRAKE_TEMP        0x00000501UL   // FL default
#define PID_BRAKE_STATUS      0x000005F1UL   // optional debug/status PID

// Suggested PID map:
// FL: 0x00000501
// FR: 0x00000502
// RL: 0x00000503
// RR: 0x00000504

// ----- RaceChrono BLE transmit -----
#define ENABLE_BLE_RACECHRONO true
#define SEND_STATUS_PID       true

// ----- Sensor behavior -----
#define USE_50HZ_FILTER       false   // false = 60Hz, true = 50Hz
#define SAMPLE_INTERVAL_MS    100     // 10Hz sensor read
#define BLE_FRAME_INTERVAL_MS 50      // 20Hz BLE frame slot, each PID about 10Hz when status PID enabled
#define SERIAL_INTERVAL_MS    1000

// Fault protection:
// transient MAX31856 fault will hold last valid temp instead of sending 0 immediately.
#define FAULT_HOLD_MS         3000

// Filtering:
// higher alpha = faster response, lower alpha = smoother
#define TEMP_FILTER_ALPHA     0.45f

// Plausible range for K-type brake measurement
#define MIN_VALID_TC_C        -40.0f
#define MAX_VALID_TC_C        1250.0f

// LED behavior
#define LED_ACTIVE_LOW        true

// ============================================================
// Pin map
// ============================================================

static const int PIN_MAX_MOSI = 6;
static const int PIN_MAX_MISO = 7;
static const int PIN_MAX_SCK  = 8;
static const int PIN_MAX_CS   = 10;

static const int PIN_LED_STAT = 2;
static const int PIN_BTN      = 3;    // reserved / currently unused

// ============================================================
// MAX31856
// ============================================================

Adafruit_MAX31856 max31856(
  PIN_MAX_CS,
  PIN_MAX_MOSI,
  PIN_MAX_MISO,
  PIN_MAX_SCK
);

// ============================================================
// RaceChrono BLE DIY CAN UUIDs
// ============================================================

static const char *UUID_RC_SERVICE =
  "00001ff8-0000-1000-8000-00805f9b34fb";
static const char *UUID_RC_CAN_MAIN =
  "00000001-0000-1000-8000-00805f9b34fb";
static const char *UUID_RC_CAN_FILTER =
  "00000002-0000-1000-8000-00805f9b34fb";

BLEServer *bleServer = nullptr;
BLECharacteristic *canMainChar = nullptr;
BLECharacteristic *canFilterChar = nullptr;

volatile bool bleConnected = false;
volatile bool bleRestartAdvertisingPending = false;

// RaceChrono may write filters. We support them, but default is allow all.
static bool filterAllowAll = true;
static bool filterDenyAllReceived = false;
static uint32_t allowedPids[16];
static uint8_t allowedPidCount = 0;

// ============================================================
// Runtime state
// ============================================================

struct TempState {
  float rawTcC = NAN;
  float filteredTcC = NAN;
  float cjC = NAN;

  float lastValidTcC = NAN;
  uint32_t lastValidMs = 0;

  bool hwValid = false;
  bool outputHeld = false;
  bool hasEverValid = false;

  uint8_t faultRaw = 0;
  uint16_t sampleCounter = 0;
};

TempState ts;

uint32_t lastSampleMs = 0;
uint32_t lastBleMs = 0;
uint32_t lastSerialMs = 0;
uint32_t lastLedMs = 0;
uint8_t ledPhase = 0;
uint8_t bleFrameSlot = 0;

// ============================================================
// Utility
// ============================================================

static inline void ledWrite(bool on) {
  if (LED_ACTIVE_LOW) {
    digitalWrite(PIN_LED_STAT, on ? LOW : HIGH);
  } else {
    digitalWrite(PIN_LED_STAT, on ? HIGH : LOW);
  }
}

static uint16_t clampTempToX10(float c) {
  if (!isfinite(c)) return 0;
  float x = c * 10.0f;
  if (x < 0.0f) x = 0.0f;
  if (x > 65535.0f) x = 65535.0f;
  return (uint16_t)lroundf(x);
}

static int16_t clampSignedTempToX10(float c) {
  if (!isfinite(c)) return 0;
  float x = c * 10.0f;
  if (x < -32768.0f) x = -32768.0f;
  if (x > 32767.0f) x = 32767.0f;
  return (int16_t)lroundf(x);
}

static void putU16BE(uint8_t *buf, uint16_t v) {
  buf[0] = (uint8_t)((v >> 8) & 0xFF);
  buf[1] = (uint8_t)(v & 0xFF);
}

static void putI16BE(uint8_t *buf, int16_t v) {
  buf[0] = (uint8_t)(((uint16_t)v >> 8) & 0xFF);
  buf[1] = (uint8_t)((uint16_t)v & 0xFF);
}

static uint16_t readU16BE(const uint8_t *p) {
  return ((uint16_t)p[0] << 8) | p[1];
}

static uint32_t readU32LE(const uint8_t *p) {
  return ((uint32_t)p[0]) |
         ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) |
         ((uint32_t)p[3] << 24);
}

static uint32_t readU32BE(const uint8_t *p) {
  return ((uint32_t)p[0] << 24) |
         ((uint32_t)p[1] << 16) |
         ((uint32_t)p[2] << 8) |
         ((uint32_t)p[3]);
}

static bool pidAllowed(uint32_t pid) {
  if (filterAllowAll) return true;

  // If RaceChrono has not sent any allow rule yet, keep sending.
  // This prevents "connected but no data" during early testing.
  if (!filterDenyAllReceived && allowedPidCount == 0) return true;

  for (uint8_t i = 0; i < allowedPidCount; i++) {
    if (allowedPids[i] == pid) return true;
  }

  return false;
}

static void addAllowedPid(uint32_t pid) {
  for (uint8_t i = 0; i < allowedPidCount; i++) {
    if (allowedPids[i] == pid) return;
  }
  if (allowedPidCount < 16) {
    allowedPids[allowedPidCount++] = pid;
  }
}

static void printFaultDetail(uint8_t f) {
  if (f == 0) {
    Serial.println("Fault detail  : NONE");
    return;
  }

  Serial.print("Fault detail  : ");
  if (f & MAX31856_FAULT_CJRANGE) Serial.print("CJRANGE ");
  if (f & MAX31856_FAULT_TCRANGE) Serial.print("TCRANGE ");
  if (f & MAX31856_FAULT_CJHIGH)  Serial.print("CJHIGH ");
  if (f & MAX31856_FAULT_CJLOW)   Serial.print("CJLOW ");
  if (f & MAX31856_FAULT_TCHIGH)  Serial.print("TCHIGH ");
  if (f & MAX31856_FAULT_TCLOW)   Serial.print("TCLOW ");
  if (f & MAX31856_FAULT_OVUV)    Serial.print("OVUV ");
  if (f & MAX31856_FAULT_OPEN)    Serial.print("OPEN ");
  Serial.println();
}

// ============================================================
// BLE callbacks
// ============================================================

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) override {
    (void)pServer;
    bleConnected = true;
    bleRestartAdvertisingPending = false;
    bleFrameSlot = 0;
    Serial.println("[BLE] RaceChrono connected");
  }

  void onDisconnect(BLEServer *pServer) override {
    (void)pServer;
    bleConnected = false;
    bleRestartAdvertisingPending = true;
    Serial.println("[BLE] disconnected");
  }
};

class FilterCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) override {
    String value = pCharacteristic->getValue();
    const uint8_t *d = (const uint8_t *)value.c_str();
    size_t n = value.length();

    if (n < 1) return;

    const uint8_t cmd = d[0];

    if (cmd == 0) {
      // Deny all
      filterAllowAll = false;
      filterDenyAllReceived = true;
      allowedPidCount = 0;
      Serial.println("[BLE] CAN filter: deny all");
      return;
    }

    if (cmd == 1) {
      // Allow all
      filterAllowAll = true;
      filterDenyAllReceived = false;
      allowedPidCount = 0;

      uint16_t interval = 0;
      if (n >= 3) interval = readU16BE(&d[1]);

      Serial.print("[BLE] CAN filter: allow all, interval=");
      Serial.println(interval);
      return;
    }

    if (cmd == 2) {
      // Allow one PID
      // Docs use PID in filter command; different examples have used different byte order.
      // Add both BE and LE parsed values to avoid silent no-data in RaceChrono.
      if (n >= 7) {
        uint16_t interval = readU16BE(&d[1]);
        uint32_t pidBE = readU32BE(&d[3]);
        uint32_t pidLE = readU32LE(&d[3]);

        addAllowedPid(pidBE);
        addAllowedPid(pidLE);
        filterAllowAll = false;
        filterDenyAllReceived = true;

        Serial.print("[BLE] CAN filter: allow PID BE=0x");
        Serial.print(pidBE, HEX);
        Serial.print(" LE=0x");
        Serial.print(pidLE, HEX);
        Serial.print(" interval=");
        Serial.println(interval);
      }
      return;
    }

    Serial.print("[BLE] unknown CAN filter cmd=");
    Serial.println(cmd);
  }
};

// ============================================================
// BLE RaceChrono functions
// ============================================================

static void bleSendCanPacket(uint32_t pid, const uint8_t *payload, uint8_t payloadLen) {
#if ENABLE_BLE_RACECHRONO
  if (!bleConnected || canMainChar == nullptr) return;
  if (!pidAllowed(pid)) return;
  if (payloadLen > 16) payloadLen = 16;

  uint8_t packet[20];
  packet[0] = (uint8_t)(pid & 0xFF);
  packet[1] = (uint8_t)((pid >> 8) & 0xFF);
  packet[2] = (uint8_t)((pid >> 16) & 0xFF);
  packet[3] = (uint8_t)((pid >> 24) & 0xFF);

  for (uint8_t i = 0; i < payloadLen; i++) {
    packet[4 + i] = payload[i];
  }

  canMainChar->setValue(packet, 4 + payloadLen);
  canMainChar->notify();
#endif
}

static void setupBleRaceChrono() {
#if ENABLE_BLE_RACECHRONO
  Serial.println("[BLE] init RaceChrono DIY CAN");

  BLEDevice::init(BLE_DEVICE_NAME);
  BLEDevice::setMTU(64);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService *service = bleServer->createService(UUID_RC_SERVICE);

  canMainChar = service->createCharacteristic(
    UUID_RC_CAN_MAIN,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_NOTIFY
  );
  canMainChar->addDescriptor(new BLE2902());

  canFilterChar = service->createCharacteristic(
    UUID_RC_CAN_FILTER,
    BLECharacteristic::PROPERTY_WRITE
  );
  canFilterChar->setCallbacks(new FilterCallbacks());

  uint8_t initPayload[8] = {0};
  bleSendCanPacket(PID_BRAKE_TEMP, initPayload, 8);

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(UUID_RC_SERVICE);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.print("[BLE] advertising name: ");
  Serial.println(BLE_DEVICE_NAME);
  Serial.print("[BLE] temp PID: 0x");
  Serial.println(PID_BRAKE_TEMP, HEX);
#endif
}

static void handleBleAdvertising() {
#if ENABLE_BLE_RACECHRONO
  if (!bleRestartAdvertisingPending) return;

  bleRestartAdvertisingPending = false;
  BLEDevice::startAdvertising();
#endif
}

// ============================================================
// MAX31856 functions
// ============================================================

static bool setupMax31856() {
  Serial.println("[MAX31856] init");

  pinMode(PIN_MAX_CS, OUTPUT);
  digitalWrite(PIN_MAX_CS, HIGH);

  bool ok = max31856.begin();

  if (!ok) {
    Serial.println("[MAX31856] begin FAILED");
    return false;
  }

  max31856.setThermocoupleType(MAX31856_TCTYPE_K);

#if USE_50HZ_FILTER
  max31856.setNoiseFilter(MAX31856_NOISE_FILTER_50HZ);
  Serial.println("[MAX31856] noise filter: 50Hz");
#else
  max31856.setNoiseFilter(MAX31856_NOISE_FILTER_60HZ);
  Serial.println("[MAX31856] noise filter: 60Hz");
#endif

  max31856.setConversionMode(MAX31856_CONTINUOUS);

  Serial.println("[MAX31856] OK");
  Serial.println("[MAX31856] type: K");
  return true;
}

static void readTemperature() {
  ts.sampleCounter++;

  float cj = max31856.readCJTemperature();
  float tc = max31856.readThermocoupleTemperature();
  uint8_t fault = max31856.readFault();

  ts.cjC = cj;
  ts.rawTcC = tc;
  ts.faultRaw = fault;

  bool finiteOk = isfinite(tc) && isfinite(cj);
  bool rangeOk = (tc >= MIN_VALID_TC_C && tc <= MAX_VALID_TC_C);
  bool faultOk = (fault == 0);

  ts.hwValid = finiteOk && rangeOk && faultOk;
  ts.outputHeld = false;

  if (ts.hwValid) {
    if (!isfinite(ts.filteredTcC)) {
      ts.filteredTcC = tc;
    } else {
      ts.filteredTcC = (TEMP_FILTER_ALPHA * tc) +
                       ((1.0f - TEMP_FILTER_ALPHA) * ts.filteredTcC);
    }

    ts.lastValidTcC = ts.filteredTcC;
    ts.lastValidMs = millis();
    ts.hasEverValid = true;
  } else {
    // Keep filtered value from last valid sample.
    if (ts.hasEverValid && (millis() - ts.lastValidMs <= FAULT_HOLD_MS)) {
      ts.outputHeld = true;
      ts.filteredTcC = ts.lastValidTcC;
    }
  }
}

static float temperatureForRaceChrono() {
  if (ts.hwValid) return ts.filteredTcC;

  if (ts.outputHeld && ts.hasEverValid) {
    return ts.lastValidTcC;
  }

  return 0.0f;
}

static uint8_t statusByte() {
  uint8_t s = 0;
  if (ts.hwValid) s |= 0x01;
  if (ts.outputHeld) s |= 0x02;
  if (!ts.hasEverValid) s |= 0x04;
  return s;
}

static void buildTemperaturePayload(uint8_t *payload) {
  float outC = temperatureForRaceChrono();

  uint16_t tempX10 = clampTempToX10(outC);
  int16_t cjX10 = clampSignedTempToX10(ts.cjC);

  putU16BE(&payload[0], tempX10);
  payload[2] = ts.faultRaw;
  payload[3] = statusByte();
  putI16BE(&payload[4], cjX10);
  putU16BE(&payload[6], ts.sampleCounter);
}

static void buildStatusPayload(uint8_t *payload) {
  int16_t cjX10 = clampSignedTempToX10(ts.cjC);

  payload[0] = ts.faultRaw;
  payload[1] = statusByte();
  payload[2] = bleConnected ? 1 : 0;
  payload[3] = 0;
  putI16BE(&payload[4], cjX10);
  putU16BE(&payload[6], ts.sampleCounter);
}

static void sendRaceChronoFrameSlot() {
  if (!bleConnected || canMainChar == nullptr) return;

  uint8_t payload[8];

#if SEND_STATUS_PID
  if (bleFrameSlot == 0) {
    buildTemperaturePayload(payload);
    bleSendCanPacket(PID_BRAKE_TEMP, payload, 8);
  } else {
    buildStatusPayload(payload);
    bleSendCanPacket(PID_BRAKE_STATUS, payload, 8);
  }
  bleFrameSlot ^= 1;
#else
  buildTemperaturePayload(payload);
  bleSendCanPacket(PID_BRAKE_TEMP, payload, 8);
#endif
}

// ============================================================
// LED behavior
// ============================================================

static void updateLed() {
  uint32_t now = millis();

  if (!bleConnected) {
    // Not connected: slow blink
    if (now - lastLedMs >= 500) {
      lastLedMs = now;
      ledPhase ^= 1;
      ledWrite(ledPhase);
    }
    return;
  }

  if (!ts.hwValid) {
    // Fault / held: fast blink
    if (now - lastLedMs >= 120) {
      lastLedMs = now;
      ledPhase ^= 1;
      ledWrite(ledPhase);
    }
    return;
  }

  // Connected and valid: solid on
  ledWrite(true);
}

// ============================================================
// Serial debug
// ============================================================

static void printStatus() {
  Serial.println("--------------------------------------------------");
  Serial.print("BLE           : ");
  Serial.println(bleConnected ? "CONNECTED" : "ADVERTISING");

  Serial.print("Raw TC        : ");
  if (isfinite(ts.rawTcC)) Serial.print(ts.rawTcC, 2);
  else Serial.print("nan");
  Serial.println(" C");

  Serial.print("Filtered TC   : ");
  if (isfinite(ts.filteredTcC)) Serial.print(ts.filteredTcC, 2);
  else Serial.print("nan");
  Serial.println(" C");

  Serial.print("RaceChrono TC : ");
  Serial.print(temperatureForRaceChrono(), 2);
  Serial.println(" C");

  Serial.print("Cold Junction : ");
  if (isfinite(ts.cjC)) Serial.print(ts.cjC, 2);
  else Serial.print("nan");
  Serial.println(" C");

  Serial.print("Valid now     : ");
  Serial.println(ts.hwValid ? "YES" : "NO");

  Serial.print("Output held   : ");
  Serial.println(ts.outputHeld ? "YES" : "NO");

  Serial.print("Fault Raw     : 0x");
  if (ts.faultRaw < 0x10) Serial.print("0");
  Serial.println(ts.faultRaw, HEX);

  printFaultDetail(ts.faultRaw);

  Serial.print("Sample        : ");
  Serial.println(ts.sampleCounter);
}

// ============================================================
// Setup / Loop
// ============================================================

void setup() {
  pinMode(PIN_LED_STAT, OUTPUT);
  ledWrite(false);

  pinMode(PIN_BTN, INPUT);

  Serial.begin(115200);
  delay(800);

  Serial.println();
  Serial.println("==================================================");
  Serial.println("Brake Temp Node 12V MAX31856 v3");
  Serial.println("ESP32-C3 + MAX31856 + RaceChrono BLE DIY CAN");
  Serial.println("Hardware: brake_temp_12V_MAX31856 2026-05-22");
  Serial.println("NO BUTTON FUNCTION / 12V ACC AUTO BOOT");
  Serial.println("==================================================");

  Serial.print("PIN_MAX_MOSI = GPIO"); Serial.println(PIN_MAX_MOSI);
  Serial.print("PIN_MAX_MISO = GPIO"); Serial.println(PIN_MAX_MISO);
  Serial.print("PIN_MAX_SCK  = GPIO"); Serial.println(PIN_MAX_SCK);
  Serial.print("PIN_MAX_CS   = GPIO"); Serial.println(PIN_MAX_CS);
  Serial.print("PIN_LED_STAT = GPIO"); Serial.println(PIN_LED_STAT);
  Serial.print("BLE Name     = "); Serial.println(BLE_DEVICE_NAME);
  Serial.print("Temp PID     = 0x"); Serial.println(PID_BRAKE_TEMP, HEX);
  Serial.println("==================================================");

  bool maxOk = setupMax31856();

  if (!maxOk) {
    Serial.println("[BOOT] MAX31856 not found. Check SPI / power / CS.");
    // Continue BLE advertising so you can still see board is alive.
  }

  setupBleRaceChrono();

  lastSampleMs = millis();
  lastBleMs = millis();
  lastSerialMs = millis();
  lastLedMs = millis();

  Serial.println("[BOOT] ready");
}

void loop() {
  uint32_t now = millis();

  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    readTemperature();
  }

  handleBleAdvertising();

  if (now - lastBleMs >= BLE_FRAME_INTERVAL_MS) {
    lastBleMs = now;
    sendRaceChronoFrameSlot();
  }

  if (now - lastSerialMs >= SERIAL_INTERVAL_MS) {
    lastSerialMs = now;
    printStatus();
  }

  updateLed();

  delay(1);
}
