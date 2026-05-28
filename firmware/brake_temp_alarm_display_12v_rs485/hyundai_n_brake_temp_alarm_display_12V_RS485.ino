
/*
  Brake Temp Node 12V RS485 Alarm Display
  Board: brake_temp_alarm_display_12V_RS485
  MCU: ESP32-C3
  Display: HS13L03W2C01 / SH1106 128x64 I2C OLED
  RS485: SP3485, Modbus RTU master
  RaceChrono: BLE DIY CAN-Bus API

  Version: V1.5 Thermal Load Alarm + RaceChrono BLE
  Date: 2026-05-28

  Main changes from V1.4:
    - Instant disc surface temperature is used for display only.
    - Alarm uses thermal load model:
        Tfast = fast surface filter
        Tcore = slow heat-soak estimate
        Dose  = accumulated thermal load
      ALARM now means "must cool down", not "momentary rotor surface touched ALM".
    - RaceChrono BLE DIY CAN output added.
    - B:OFF only mutes FAIL/WARN. Real ALARM still forces buzzer unless UP+DOWN mute is active.

  Display:
    - Large single temperature number only
    - Top sensor online status
    - ALM display retained
    - Hyundai N style startup animation retained exactly from V1.4

  Key behavior:
    UP short / hold      -> ALM +10C
    DOWN short / hold    -> ALM -10C
    SET short            -> Save ALM to NVS
    SET long             -> Toggle buzzer enable for FAIL/WARN
    UP + DOWN hold 1s    -> Mute buzzer for 60s

  Hardware:
    RS485_DE      -> GPIO4
    OLED_SCL      -> GPIO5
    OLED_SDA      -> GPIO6
    BOOT          -> GPIO9
    BUZZER_CTRL   -> GPIO10
    RS485_RX      -> RXD / GPIO20
    RS485_TX      -> TXD / GPIO21
    KEY_SET       -> GPIO0
    KEY_UP        -> GPIO1
    LED_STAT      -> GPIO2, active LOW
    KEY_DOWN      -> GPIO3

  RaceChrono DIY CAN PIDs:
    0x100 Brake FL        payload[0..1] int16 big-endian, temp x10 C, 0x7FFF invalid
    0x101 Brake FR        payload[0..1] int16 big-endian, temp x10 C, 0x7FFF invalid
    0x102 Brake Display   payload[0..1] int16 big-endian, shown temp x10 C
    0x103 Brake Fast      payload[0..1] int16 big-endian, Tfast x10 C
    0x104 Brake Core      payload[0..1] int16 big-endian, Tcore x10 C
    0x105 Brake Dose      payload[0..1] uint16 big-endian, Dose x10
    0x106 Brake State     payload[0]=0 OK, 1 WARN, 2 ALARM, 3 FAIL
    0x107 Brake Alarm     payload[0..1] uint16 big-endian, ALM C
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <math.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// ================================================================
// User configuration
// ================================================================

#define ENABLE_FL 1
#define ENABLE_FR 1

#define SENSOR_FL_ADDR 0x01
#define SENSOR_FR_ADDR 0x02

#define RS485_BAUD 9600
#define RS485_SERIAL_CONFIG SERIAL_8N1

#define PIN_RS485_RX 20
#define PIN_RS485_TX 21

#define OLED_I2C_ADDR 0x3C

#define TEMP_SCALE 0.1f
#define TEMP_OFFSET 0.0f

#define TEMP_MIN_VALID_C -40.0f
#define TEMP_MAX_VALID_C 1200.0f

#define DEFAULT_ALARM_TEMP_C 600
#define MIN_ALARM_TEMP_C 100
#define MAX_ALARM_TEMP_C 900
#define ALARM_STEP_C 10

// 调试阶段没插传感器时，20ms 不会卡按键。
// 如果后续传感器读数不稳定，可以改成 50。
#define MODBUS_TIMEOUT_MS 20

#define SENSOR_POLL_INTERVAL_MS 300
#define SENSOR_STALE_MS 2500
#define DISPLAY_REFRESH_MS 100
#define BOOT_ANIMATION_ENABLE 1
#define BOOT_ANIMATION_MS 2800
#define BOOT_FRAME_MS 35
#define DEBUG_PRINT_MS 1000

#define KEY_DEBOUNCE_MS 30
#define KEY_LONG_PRESS_MS 900
#define KEY_REPEAT_START_MS 450
#define KEY_REPEAT_INTERVAL_MS 150

// ================================================================
// Thermal Load Alarm V1.5
// ================================================================

// ALM 不再代表刹车盘表面瞬时报警线，而代表“热浸透核心温度报警线”。
#define WARN_CORE_MARGIN_C 40

// 快速表面滤波时间常数。越小越跟手。
#define FAST_TEMP_TAU_SEC 0.8f

// 热浸透模型：升温比降温快，模拟刹车皮/卡钳/油液被持续热浸透。
#define CORE_HEAT_TAU_SEC 8.0f
#define CORE_COOL_TAU_SEC 18.0f

// 热负荷从 ALM - 80C 开始累积。
#define DOSE_START_OFFSET_C 80

// 热负荷报警阈值。路试后可以按实际刹车皮/赛道调整。
#define THERMAL_DOSE_WARN 10.0f
#define THERMAL_DOSE_ALARM 20.0f
#define THERMAL_DOSE_MAX 99.0f

// Core 达到 ALM 后必须持续这么久才真正报警，避免单次重刹误报。
#define ALARM_CORE_HOLD_MS 8000

// 极端峰值保护：盘面温度非常高且持续时，强制报警。
#define PEAK_CRITICAL_TEMP_C 780
#define PEAK_CRITICAL_HOLD_MS 3000

// 高峰值 + 已热浸透，提前报警。
#define PEAK_WITH_SOAK_TEMP_C 700
#define PEAK_WITH_SOAK_CORE_MARGIN_C 20
#define PEAK_WITH_SOAK_HOLD_MS 5000

// 报警解除必须 core 和 dose 都降下来。
#define CORE_ALARM_RELEASE_MARGIN_C 50
#define DOSE_RELEASE_VALUE 8.0f

// ================================================================
// RaceChrono BLE DIY CAN configuration
// ================================================================

#define ENABLE_RACECHRONO_BLE 1
#define RC_BLE_DEVICE_NAME "N BrakeTemp"
#define RC_BLE_SERVICE_UUID "00001ff8-0000-1000-8000-00805f9b34fb"
#define RC_BLE_CAN_MAIN_UUID "0001"
#define RC_BLE_CAN_FILTER_UUID "0002"

#define RC_SEND_INTERVAL_MS 200
#define RC_DEFAULT_NOTIFY_MS 200
#define RC_MAX_FILTERS 12

#define RC_PID_BRAKE_FL      0x100
#define RC_PID_BRAKE_FR      0x101
#define RC_PID_BRAKE_DISPLAY 0x102
#define RC_PID_BRAKE_FAST    0x103
#define RC_PID_BRAKE_CORE    0x104
#define RC_PID_BRAKE_DOSE    0x105
#define RC_PID_BRAKE_STATE   0x106
#define RC_PID_BRAKE_ALARM   0x107

// ================================================================
// Pin mapping
// ================================================================

static const uint8_t PIN_RS485_DE    = 4;
static const uint8_t PIN_OLED_SCL    = 5;
static const uint8_t PIN_OLED_SDA    = 6;
static const uint8_t PIN_BOOT        = 9;
static const uint8_t PIN_BUZZER_CTRL = 10;
static const uint8_t PIN_KEY_SET     = 0;
static const uint8_t PIN_KEY_UP      = 1;
static const uint8_t PIN_LED_STAT    = 2;
static const uint8_t PIN_KEY_DOWN    = 3;

// ================================================================
// OLED / Serial / Preferences
// ================================================================

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
HardwareSerial RS485Bus(1);
Preferences prefs;


static const uint8_t HYUNDAI_N_LOGO_W = 64;
static const uint8_t HYUNDAI_N_LOGO_H = 28;
static const uint8_t HYUNDAI_N_LOGO_XBM[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xC0, 0xFF, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xFF, 0x0F,
  0x00, 0x00, 0x00, 0x00, 0x00, 0xE0, 0xFF, 0x07, 0x00, 0xC0, 0x7F, 0x00,
  0x00, 0xFC, 0xFF, 0x07, 0x00, 0xC0, 0xFF, 0x01, 0x00, 0xFE, 0xFF, 0x03,
  0x00, 0xE0, 0xFF, 0x07, 0x00, 0xFE, 0xFF, 0x03, 0x00, 0xE0, 0xFF, 0x0F,
  0x00, 0xFF, 0xFF, 0x01, 0x00, 0xF0, 0xFF, 0x1F, 0x00, 0xFF, 0xFF, 0x01,
  0x00, 0xF0, 0xFF, 0x3F, 0x80, 0xFF, 0xFF, 0x00, 0x00, 0xF8, 0xFF, 0x7F,
  0x80, 0xFF, 0xFF, 0x00, 0x00, 0xF8, 0xFF, 0xFF, 0xC0, 0xFF, 0x7F, 0x00,
  0x00, 0xFC, 0xFF, 0xFF, 0xC1, 0xFF, 0x7F, 0x00, 0x00, 0xFC, 0xFF, 0xFF,
  0xE3, 0xFF, 0x3F, 0x00, 0x00, 0xFE, 0xFF, 0xFF, 0xE7, 0xFF, 0x3F, 0x00,
  0x00, 0xFE, 0x9F, 0xFF, 0xEF, 0xFF, 0x1F, 0x00, 0x00, 0xFE, 0x1F, 0xFE,
  0xFF, 0xFF, 0x1F, 0x00, 0x00, 0xFF, 0x0F, 0xFC, 0xFF, 0xFF, 0x0F, 0x00,
  0x00, 0xFF, 0x0F, 0xF0, 0xFF, 0xFF, 0x0F, 0x00, 0x80, 0xFF, 0x07, 0xE0,
  0xFF, 0xFF, 0x0F, 0x00, 0x80, 0xFF, 0x07, 0xC0, 0xFF, 0xFF, 0x07, 0x00,
  0xC0, 0xFF, 0x03, 0x80, 0xFF, 0xFF, 0x07, 0x00, 0xC0, 0xFF, 0x03, 0x00,
  0xFF, 0xFF, 0x03, 0x00, 0xE0, 0xFF, 0x01, 0x00, 0xFE, 0xFF, 0x03, 0x00,
  0xE0, 0xFF, 0x01, 0x00, 0xFC, 0xFF, 0x01, 0x00, 0xF0, 0xFF, 0x01, 0x00,
  0xF0, 0xFF, 0x01, 0x00, 0xF0, 0xFF, 0x00, 0x00, 0x80, 0xFF, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00
};

// ================================================================
// Data structures
// ================================================================

enum SystemState {
  STATE_OK = 0,
  STATE_WARN,
  STATE_ALARM,
  STATE_FAIL
};

struct ModbusCandidate {
  uint8_t functionCode;
  uint16_t regAddr;
  float scale;
  const char *name;
};

static const ModbusCandidate MODBUS_CANDIDATES[] = {
  {0x03, 0x0000, TEMP_SCALE, "03/0000"},
  {0x04, 0x0000, TEMP_SCALE, "04/0000"},
  {0x03, 0x0001, TEMP_SCALE, "03/0001"},
  {0x04, 0x0001, TEMP_SCALE, "04/0001"},
};

static const int MODBUS_CANDIDATE_COUNT =
    sizeof(MODBUS_CANDIDATES) / sizeof(MODBUS_CANDIDATES[0]);

struct BrakeSensor {
  const char *name;
  uint8_t addr;
  bool enabled;

  float tempC;
  bool ok;
  uint32_t lastOkMs;
  uint16_t failCount;

  int preferredCandidate;
  int16_t lastRaw;
};

BrakeSensor sensors[2] = {
  {"FL", SENSOR_FL_ADDR, ENABLE_FL, NAN, false, 0, 0, -1, 0},
  {"FR", SENSOR_FR_ADDR, ENABLE_FR, NAN, false, 0, 0, -1, 0},
};

struct ButtonState {
  uint8_t pin = 255;
  bool rawPressed = false;
  bool stablePressed = false;
  uint32_t lastRawChangeMs = 0;
  uint32_t pressStartMs = 0;
  uint32_t lastRepeatMs = 0;
  bool longReported = false;

  bool shortEvent = false;
  bool longEvent = false;
  bool repeatEvent = false;

  void begin(uint8_t p) {
    pin = p;
    pinMode(pin, INPUT_PULLUP);

    rawPressed = (digitalRead(pin) == LOW);
    stablePressed = rawPressed;
    lastRawChangeMs = millis();
    pressStartMs = millis();
    lastRepeatMs = millis();
    longReported = false;
  }

  void update(uint32_t now) {
    shortEvent = false;
    longEvent = false;
    repeatEvent = false;

    bool raw = (digitalRead(pin) == LOW);

    if (raw != rawPressed) {
      rawPressed = raw;
      lastRawChangeMs = now;
    }

    if ((now - lastRawChangeMs) >= KEY_DEBOUNCE_MS && stablePressed != rawPressed) {
      stablePressed = rawPressed;

      if (stablePressed) {
        pressStartMs = now;
        lastRepeatMs = now;
        longReported = false;
      } else {
        if (!longReported && (now - pressStartMs) > 40) {
          shortEvent = true;
        }
      }
    }

    if (stablePressed) {
      if (!longReported && (now - pressStartMs) >= KEY_LONG_PRESS_MS) {
        longReported = true;
        longEvent = true;
      }

      if ((now - pressStartMs) >= KEY_REPEAT_START_MS &&
          (now - lastRepeatMs) >= KEY_REPEAT_INTERVAL_MS) {
        repeatEvent = true;
        lastRepeatMs = now;
      }
    }
  }

  bool pressed() const {
    return stablePressed;
  }
};

ButtonState btnSet;
ButtonState btnUp;
ButtonState btnDown;

struct RaceChronoPidFilter {
  uint32_t pid = 0;
  uint16_t intervalMs = RC_DEFAULT_NOTIFY_MS;
  uint32_t lastSentMs = 0;
  bool used = false;
};

// ================================================================
// Runtime variables
// ================================================================

int alarmTempC = DEFAULT_ALARM_TEMP_C;
bool buzzerEnabled = true;
bool alarmLatched = false;
bool settingsDirty = false;

SystemState systemState = STATE_FAIL;

uint32_t startupMs = 0;
uint32_t lastDisplayMs = 0;
uint32_t lastPollMs = 0;
uint32_t lastDebugMs = 0;
uint32_t muteUntilMs = 0;
uint32_t lastAdjustMs = 0;
uint32_t savedNoticeUntilMs = 0;
uint32_t buzzerNoticeUntilMs = 0;

// Thermal model runtime
float tempFastC = NAN;
float tempCoreC = NAN;
float thermalDose = 0.0f;

uint32_t lastThermalUpdateMs = 0;
uint32_t coreOverAlarmSinceMs = 0;
uint32_t peakCriticalSinceMs = 0;
uint32_t peakWithSoakSinceMs = 0;
bool thermalAlarmLatched = false;

#if ENABLE_RACECHRONO_BLE
BLEServer *rcServer = nullptr;
BLECharacteristic *rcCanMainCharacteristic = nullptr;
BLECharacteristic *rcCanFilterCharacteristic = nullptr;
BLEAdvertising *rcAdvertising = nullptr;

bool rcBleConnected = false;
bool rcAllowAll = true;
uint16_t rcDefaultNotifyMs = RC_DEFAULT_NOTIFY_MS;
RaceChronoPidFilter rcFilters[RC_MAX_FILTERS];
uint32_t rcLastSendMs = 0;
uint32_t rcNotifyCount = 0;
#endif

// ================================================================
// Basic helpers
// ================================================================

static int clampInt(int v, int lo, int hi) {
  if (v < lo) return lo;
  if (v > hi) return hi;
  return v;
}

static const char *stateText(SystemState s) {
  switch (s) {
    case STATE_OK: return "OK";
    case STATE_WARN: return "WARN";
    case STATE_ALARM: return "ALARM";
    case STATE_FAIL: return "FAIL";
    default: return "--";
  }
}

static void ledSet(bool on) {
  digitalWrite(PIN_LED_STAT, on ? LOW : HIGH);
}

static void buzzerSet(bool on) {
  digitalWrite(PIN_BUZZER_CTRL, on ? HIGH : LOW);
}

static void shortBeep(uint16_t ms) {
  if (!buzzerEnabled) return;
  buzzerSet(true);
  delay(ms);
  buzzerSet(false);
}

static int16_t tempToI16x10(float t, bool valid) {
  if (!valid || isnan(t)) {
    return 0x7FFF;
  }

  float scaled = t * 10.0f;
  if (scaled > 32766.0f) scaled = 32766.0f;
  if (scaled < -32768.0f) scaled = -32768.0f;

  return (int16_t)lroundf(scaled);
}

static void writeI16BE(uint8_t *buf, int16_t v) {
  uint16_t u = (uint16_t)v;
  buf[0] = (uint8_t)(u >> 8);
  buf[1] = (uint8_t)(u & 0xFF);
}

static void writeU16BE(uint8_t *buf, uint16_t v) {
  buf[0] = (uint8_t)(v >> 8);
  buf[1] = (uint8_t)(v & 0xFF);
}

static uint16_t readU16BE(const uint8_t *buf) {
  return ((uint16_t)buf[0] << 8) | buf[1];
}

static uint32_t readU32BE(const uint8_t *buf) {
  return ((uint32_t)buf[0] << 24) |
         ((uint32_t)buf[1] << 16) |
         ((uint32_t)buf[2] << 8) |
         ((uint32_t)buf[3]);
}

// ================================================================
// Settings
// ================================================================

static void loadSettings() {
  prefs.begin("brake12v", false);

  alarmTempC = prefs.getInt("alarmC", DEFAULT_ALARM_TEMP_C);
  buzzerEnabled = prefs.getBool("buzzer", true);

  alarmTempC = clampInt(alarmTempC, MIN_ALARM_TEMP_C, MAX_ALARM_TEMP_C);
}

static void saveSettings() {
  prefs.putInt("alarmC", alarmTempC);
  prefs.putBool("buzzer", buzzerEnabled);
  settingsDirty = false;
  savedNoticeUntilMs = millis() + 2500;
}

// ================================================================
// BLE RaceChrono DIY CAN
// ================================================================

static bool pidAllowed(uint32_t pid) {
  if (filterAllowAll) return true;
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

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    (void)server;
    bleConnected = true;
    bleRestartAdvertisingPending = false;
    Serial.println("[BLE] RaceChrono connected");
  }

  void onDisconnect(BLEServer *server) override {
    (void)server;
    bleConnected = false;
    bleRestartAdvertisingPending = true;
    Serial.println("[BLE] disconnected");
  }
};

class FilterCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    String value = characteristic->getValue();
    const uint8_t *d = (const uint8_t *)value.c_str();
    size_t n = value.length();

    if (n < 1) return;

    const uint8_t cmd = d[0];

    if (cmd == 0) {
      filterAllowAll = false;
      filterDenyAllReceived = true;
      allowedPidCount = 0;
      Serial.println("[BLE] CAN filter: deny all");
      return;
    }

    if (cmd == 1) {
      filterAllowAll = true;
      filterDenyAllReceived = false;
      allowedPidCount = 0;
      Serial.println("[BLE] CAN filter: allow all");
      return;
    }

    if (cmd == 2 && n >= 7) {
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
      return;
    }
  }
};

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
#else
  (void)pid;
  (void)payload;
  (void)payloadLen;
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

  service->start();

  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(UUID_RC_SERVICE);
  adv->setScanResponse(true);
  adv->setMinPreferred(0x06);
  adv->setMinPreferred(0x12);

  BLEDevice::startAdvertising();

  Serial.print("[BLE] advertising name: ");
  Serial.println(BLE_DEVICE_NAME);
  Serial.println("[BLE] Service UUID: 0x1FF8, CAN main: 0x0001, filter: 0x0002");
  Serial.print("[BLE] BrakeDiscTempMax PID: 0x");
  Serial.println(PID_BRAKE_DISC_TEMP_MAX, HEX);
  Serial.print("[BLE] BrakeDiscTempFL  PID: 0x");
  Serial.println(PID_BRAKE_DISC_TEMP_FL, HEX);
  Serial.print("[BLE] BrakeDiscTempFR  PID: 0x");
  Serial.println(PID_BRAKE_DISC_TEMP_FR, HEX);
#endif
}

static void handleBleAdvertising() {
#if ENABLE_BLE_RACECHRONO
  if (!bleRestartAdvertisingPending) return;
  bleRestartAdvertisingPending = false;
  BLEDevice::startAdvertising();
  Serial.println("[BLE] advertising restarted");
#endif
}

// ================================================================
// RS485 / Modbus
// ================================================================

static uint16_t modbusCRC16(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;

  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];

    for (uint8_t b = 0; b < 8; b++) {
      if (crc & 0x0001) {
        crc >>= 1;
        crc ^= 0xA001;
      } else {
        crc >>= 1;
      }
    }
  }

  return crc;
}

static void rs485ReceiveMode() {
  digitalWrite(PIN_RS485_DE, LOW);
}

static void rs485TransmitMode() {
  digitalWrite(PIN_RS485_DE, HIGH);
}

static void rs485ClearInput() {
  while (RS485Bus.available()) {
    RS485Bus.read();
  }
}

static bool parseModbusReadResponse(uint8_t addr,
                                    uint8_t functionCode,
                                    const uint8_t *buf,
                                    size_t len,
                                    uint16_t &valueOut) {
  for (size_t i = 0; i + 4 < len; i++) {
    if (buf[i] != addr) continue;

    if (buf[i + 1] == (uint8_t)(functionCode | 0x80)) {
      return false;
    }

    if (buf[i + 1] != functionCode) continue;

    uint8_t byteCount = buf[i + 2];
    if (byteCount < 2) continue;

    size_t frameLen = 3 + byteCount + 2;
    if (i + frameLen > len) continue;

    uint16_t rxCrc = (uint16_t)buf[i + frameLen - 2] |
                     ((uint16_t)buf[i + frameLen - 1] << 8);
    uint16_t calcCrc = modbusCRC16(&buf[i], frameLen - 2);

    if (rxCrc != calcCrc) continue;

    valueOut = ((uint16_t)buf[i + 3] << 8) | buf[i + 4];
    return true;
  }

  return false;
}

static bool modbusReadOneRegister(uint8_t addr,
                                  uint8_t functionCode,
                                  uint16_t regAddr,
                                  uint16_t &valueOut) {
  uint8_t req[8];

  req[0] = addr;
  req[1] = functionCode;
  req[2] = (uint8_t)(regAddr >> 8);
  req[3] = (uint8_t)(regAddr & 0xFF);
  req[4] = 0x00;
  req[5] = 0x01;

  uint16_t crc = modbusCRC16(req, 6);
  req[6] = (uint8_t)(crc & 0xFF);
  req[7] = (uint8_t)(crc >> 8);

  rs485ClearInput();

  rs485TransmitMode();
  delayMicroseconds(100);
  RS485Bus.write(req, sizeof(req));
  RS485Bus.flush();
  delayMicroseconds(180);
  rs485ReceiveMode();

  uint8_t resp[32];
  size_t n = 0;
  uint32_t start = millis();

  while ((millis() - start) < MODBUS_TIMEOUT_MS && n < sizeof(resp)) {
    int c = RS485Bus.read();

    if (c >= 0) {
      resp[n++] = (uint8_t)c;
    } else {
      delay(0);
    }
  }

  if (n < 5) {
    return false;
  }

  return parseModbusReadResponse(addr, functionCode, resp, n, valueOut);
}

static bool tryReadTempWithCandidate(uint8_t addr,
                                     const ModbusCandidate &cand,
                                     float &tempCOut,
                                     int16_t &rawOut) {
  uint16_t rawU = 0;

  if (!modbusReadOneRegister(addr, cand.functionCode, cand.regAddr, rawU)) {
    return false;
  }

  int16_t rawS = (int16_t)rawU;
  float t = ((float)rawS * cand.scale) + TEMP_OFFSET;

  if (t < TEMP_MIN_VALID_C || t > TEMP_MAX_VALID_C) {
    return false;
  }

  tempCOut = t;
  rawOut = rawS;
  return true;
}

static bool applyTemperatureFilter(BrakeSensor &s, float rawC, float &filteredOut) {
  if (!isfinite(rawC)) return false;

  if (!s.hasFiltered || !isfinite(s.filteredTempC)) {
    s.filteredTempC = rawC;
    s.hasFiltered = true;
  } else {
    float delta = rawC - s.filteredTempC;

    if (fabsf(delta) > TEMP_SPIKE_GATE_C) {
      Serial.print("[RS485] ");
      Serial.print(s.name);
      Serial.print(" spike rejected raw=");
      Serial.print(rawC, 1);
      Serial.print("C filt=");
      Serial.print(s.filteredTempC, 1);
      Serial.println("C");
      return false;
    }

    s.filteredTempC += delta * TEMP_FILTER_ALPHA;
  }

  filteredOut = s.filteredTempC;
  return true;
}

static bool readBrakeTemperature(BrakeSensor &s, float &tempCOut, int16_t &rawOut) {
  if (!s.enabled) return false;

  if (s.preferredCandidate >= 0 && s.preferredCandidate < MODBUS_CANDIDATE_COUNT) {
    if (tryReadTempWithCandidate(s.addr,
                                 MODBUS_CANDIDATES[s.preferredCandidate],
                                 tempCOut,
                                 rawOut)) {
      return true;
    }
  }

  int i = s.probeCandidate;
  if (i < 0 || i >= MODBUS_CANDIDATE_COUNT) i = 0;
  s.probeCandidate = (i + 1) % MODBUS_CANDIDATE_COUNT;

  if (i != s.preferredCandidate &&
      tryReadTempWithCandidate(s.addr, MODBUS_CANDIDATES[i], tempCOut, rawOut)) {
    s.preferredCandidate = i;
    return true;
  }

  return false;
}

// ================================================================
// Sensor polling
// ================================================================

static void pollSensors(uint32_t now) {
  if ((now - lastPollMs) < SENSOR_POLL_INTERVAL_MS) {
    return;
  }

  lastPollMs = now;

  static uint8_t pollIndex = 0;
  BrakeSensor &s = sensors[pollIndex];
  pollIndex = (pollIndex + 1) % 2;

  if (!s.enabled) {
    s.ok = false;
    return;
  }

  float t = NAN;
  int16_t raw = 0;

  bool ok = readBrakeTemperature(s, t, raw);

  if (ok) {
    float filtered = NAN;

    if (applyTemperatureFilter(s, t, filtered)) {
      s.tempC = filtered;
      s.lastRaw = raw;
      s.ok = true;
      s.lastOkMs = now;
      s.failCount = 0;
      s.sampleCounter++;
    } else {
      s.failCount++;
      if (s.lastOkMs == 0 || (now - s.lastOkMs) > SENSOR_STALE_MS) {
        s.ok = false;
      }
    }
  } else {
    s.failCount++;

    if (s.lastOkMs == 0 || (now - s.lastOkMs) > SENSOR_STALE_MS) {
      s.ok = false;
    }

    if (s.failCount >= 10) {
      s.preferredCandidate = -1;
    }
  }
}

// ================================================================
// Temperature / Alarm logic
// ================================================================

static float getDisplayTemp(bool &anyOkOut) {
  anyOkOut = false;
  float displayT = -9999.0f;

  for (int i = 0; i < 2; i++) {
    if (!sensors[i].enabled) continue;

    if (sensors[i].ok && !isnan(sensors[i].tempC)) {
      anyOkOut = true;

      // 两个节点都在线时，显示较高温度；但屏幕上不显示 MAX 字样。
      if (sensors[i].tempC > displayT) {
        displayT = sensors[i].tempC;
      }
    }
  }

  if (!anyOkOut) return NAN;
  return displayT;
}

static bool hasAnySensorFail() {
  for (int i = 0; i < 2; i++) {
    if (!sensors[i].enabled) continue;
    if (!sensors[i].ok) return true;
  }

  return false;
}

static void resetThermalModel(uint32_t now) {
  tempFastC = NAN;
  tempCoreC = NAN;
  thermalDose = 0.0f;
  lastThermalUpdateMs = now;
  coreOverAlarmSinceMs = 0;
  peakCriticalSinceMs = 0;
  peakWithSoakSinceMs = 0;
  thermalAlarmLatched = false;
}

static void updateThermalModel(uint32_t now) {
  bool anyOk = false;
  float rawT = getDisplayTemp(anyOk);

  if (!anyOk || isnan(rawT)) {
    resetThermalModel(now);
    return;
  }

  if (lastThermalUpdateMs == 0 || isnan(tempFastC) || isnan(tempCoreC)) {
    lastThermalUpdateMs = now;
    tempFastC = rawT;
    tempCoreC = rawT;
    thermalDose = 0.0f;
    return;
  }

  float dt = (now - lastThermalUpdateMs) / 1000.0f;
  lastThermalUpdateMs = now;

  if (dt <= 0.0f || dt > 2.0f) {
    dt = 0.1f;
  }

  // 快速滤波，跟随刹车盘表面变化。
  float alphaFast = dt / (FAST_TEMP_TAU_SEC + dt);
  tempFastC += alphaFast * (rawT - tempFastC);

  // 慢速热浸透模型：升温较快，降温较慢。
  float tauCore = (rawT > tempCoreC) ? CORE_HEAT_TAU_SEC : CORE_COOL_TAU_SEC;
  float alphaCore = dt / (tauCore + dt);
  tempCoreC += alphaCore * (rawT - tempCoreC);

  // 热负荷积分：只在 ALM - 80°C 以上开始累积。
  float doseStartC = (float)(alarmTempC - DOSE_START_OFFSET_C);

  if (tempCoreC > doseStartC) {
    float intensity = (tempCoreC - doseStartC) / (float)DOSE_START_OFFSET_C;

    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 2.5f) intensity = 2.5f;

    thermalDose += intensity * dt;
  } else {
    // 冷却时热负荷慢慢消退。
    thermalDose -= dt * 0.7f;
  }

  if (thermalDose < 0.0f) thermalDose = 0.0f;
  if (thermalDose > THERMAL_DOSE_MAX) thermalDose = THERMAL_DOSE_MAX;
}

static void updateAlarmState(uint32_t now) {
  bool anyOk = false;
  float rawT = getDisplayTemp(anyOk);
  bool anyFail = hasAnySensorFail();

  if (!anyOk || isnan(rawT) || isnan(tempFastC) || isnan(tempCoreC)) {
    alarmLatched = false;
    thermalAlarmLatched = false;
    systemState = STATE_FAIL;
    return;
  }

  // 1. Core 温度达到 ALM，必须持续一段时间才报警。
  if (tempCoreC >= (float)alarmTempC) {
    if (coreOverAlarmSinceMs == 0) {
      coreOverAlarmSinceMs = now;
    }
  } else {
    coreOverAlarmSinceMs = 0;
  }

  bool coreSustainedAlarm =
      (coreOverAlarmSinceMs != 0 &&
       (now - coreOverAlarmSinceMs) >= ALARM_CORE_HOLD_MS);

  // 2. 极端表面峰值保护。
  if (tempFastC >= (float)PEAK_CRITICAL_TEMP_C) {
    if (peakCriticalSinceMs == 0) {
      peakCriticalSinceMs = now;
    }
  } else {
    peakCriticalSinceMs = 0;
  }

  bool peakCriticalAlarm =
      (peakCriticalSinceMs != 0 &&
       (now - peakCriticalSinceMs) >= PEAK_CRITICAL_HOLD_MS);

  // 3. 表面温度很高，并且 core 已经接近 ALM。
  if (tempFastC >= (float)PEAK_WITH_SOAK_TEMP_C &&
      tempCoreC >= (float)(alarmTempC - PEAK_WITH_SOAK_CORE_MARGIN_C)) {
    if (peakWithSoakSinceMs == 0) {
      peakWithSoakSinceMs = now;
    }
  } else {
    peakWithSoakSinceMs = 0;
  }

  bool peakWithSoakAlarm =
      (peakWithSoakSinceMs != 0 &&
       (now - peakWithSoakSinceMs) >= PEAK_WITH_SOAK_HOLD_MS);

  // 4. 热负荷积分报警。
  bool doseAlarm = thermalDose >= THERMAL_DOSE_ALARM;

  if (coreSustainedAlarm || peakCriticalAlarm || peakWithSoakAlarm || doseAlarm) {
    thermalAlarmLatched = true;
    alarmLatched = true;
  }

  // 解除报警需要 core 和 dose 都降下来，避免刚降一点就解除。
  if (thermalAlarmLatched) {
    bool coreReleased = tempCoreC <= (float)(alarmTempC - CORE_ALARM_RELEASE_MARGIN_C);
    bool doseReleased = thermalDose <= DOSE_RELEASE_VALUE;
    bool peakReleased = tempFastC < (float)(alarmTempC - 20);

    if (coreReleased && doseReleased && peakReleased) {
      thermalAlarmLatched = false;
      alarmLatched = false;
      muteUntilMs = 0;
    }
  }

  if (thermalAlarmLatched) {
    systemState = STATE_ALARM;
    return;
  }

  bool warnByCore = tempCoreC >= (float)(alarmTempC - WARN_CORE_MARGIN_C);
  bool warnByDose = thermalDose >= THERMAL_DOSE_WARN;

  if (warnByCore || warnByDose) {
    systemState = STATE_WARN;
  } else if (anyFail) {
    systemState = STATE_FAIL;
  } else {
    systemState = STATE_OK;
  }
}

// ================================================================
// RaceChrono BLE DIY CAN
// ================================================================

#if ENABLE_RACECHRONO_BLE

static void rcClearFilters() {
  for (int i = 0; i < RC_MAX_FILTERS; i++) {
    rcFilters[i].used = false;
    rcFilters[i].pid = 0;
    rcFilters[i].intervalMs = RC_DEFAULT_NOTIFY_MS;
    rcFilters[i].lastSentMs = 0;
  }
}

static RaceChronoPidFilter *rcFindFilter(uint32_t pid) {
  for (int i = 0; i < RC_MAX_FILTERS; i++) {
    if (rcFilters[i].used && rcFilters[i].pid == pid) {
      return &rcFilters[i];
    }
  }
  return nullptr;
}

static RaceChronoPidFilter *rcAddFilter(uint32_t pid, uint16_t intervalMs) {
  RaceChronoPidFilter *existing = rcFindFilter(pid);
  if (existing != nullptr) {
    existing->intervalMs = intervalMs;
    return existing;
  }

  for (int i = 0; i < RC_MAX_FILTERS; i++) {
    if (!rcFilters[i].used) {
      rcFilters[i].used = true;
      rcFilters[i].pid = pid;
      rcFilters[i].intervalMs = intervalMs;
      rcFilters[i].lastSentMs = 0;
      return &rcFilters[i];
    }
  }

  return nullptr;
}

static bool rcShouldSendPid(uint32_t pid, uint32_t now) {
  if (rcAllowAll) {
    // rcBleUpdate() already limits the global send period.
    // In allow-all mode we want every telemetry PID to be sent in the same update burst.
    (void)pid;
    (void)now;
    return true;
  }

  RaceChronoPidFilter *f = rcFindFilter(pid);
  if (f == nullptr) {
    return false;
  }

  uint16_t interval = f->intervalMs;
  if (interval == 0) interval = RC_DEFAULT_NOTIFY_MS;

  if ((now - f->lastSentMs) >= interval) {
    f->lastSentMs = now;
    return true;
  }

  return false;
}

class RaceChronoServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *server) override {
    (void)server;
    rcBleConnected = true;
    Serial.println("[BLE] RaceChrono connected");
  }

  void onDisconnect(BLEServer *server) override {
    (void)server;
    rcBleConnected = false;
    Serial.println("[BLE] RaceChrono disconnected, advertising restarted");
    BLEDevice::startAdvertising();
  }
};

class RaceChronoFilterCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *characteristic) override {
    auto value = characteristic->getValue();
    size_t len = value.length();

    if (len < 1) {
      return;
    }

    uint8_t cmd = (uint8_t)value[0];

    Serial.print("[BLE] Filter cmd: ");
    for (size_t i = 0; i < len; i++) {
      Serial.printf("%02X ", (uint8_t)value[i]);
    }
    Serial.println();

    if (cmd == 0x00 && len == 1) {
      // Deny all
      rcAllowAll = false;
      rcClearFilters();
      Serial.println("[BLE] CAN filter: deny all");
      return;
    }

    if (cmd == 0x01 && len >= 3) {
      // Allow all
      rcAllowAll = true;
      rcDefaultNotifyMs = readU16BE((const uint8_t *)&value[1]);
      if (rcDefaultNotifyMs == 0) rcDefaultNotifyMs = RC_DEFAULT_NOTIFY_MS;
      rcClearFilters();

      Serial.print("[BLE] CAN filter: allow all, interval=");
      Serial.print(rcDefaultNotifyMs);
      Serial.println("ms");
      return;
    }

    if (cmd == 0x02 && len >= 7) {
      // Allow one PID. RaceChrono API encodes filter values big-endian.
      uint16_t intervalMs = readU16BE((const uint8_t *)&value[1]);
      uint32_t pid = readU32BE((const uint8_t *)&value[3]);

      if (intervalMs == 0) intervalMs = RC_DEFAULT_NOTIFY_MS;
      rcAllowAll = false;
      rcAddFilter(pid, intervalMs);

      Serial.print("[BLE] CAN filter: allow PID 0x");
      Serial.print(pid, HEX);
      Serial.print(", interval=");
      Serial.print(intervalMs);
      Serial.println("ms");
      return;
    }
  }
};

static void rcStartAdvertising() {
  rcAdvertising = BLEDevice::getAdvertising();
  rcAdvertising->addServiceUUID(RC_BLE_SERVICE_UUID);
  rcAdvertising->setScanResponse(true);
  rcAdvertising->setMinPreferred(0x06);
  rcAdvertising->setMaxPreferred(0x12);
  rcAdvertising->start();
}

static void rcBleBegin() {
  Serial.println("[BLE] Init RaceChrono BLE DIY CAN");

  BLEDevice::init(RC_BLE_DEVICE_NAME);

#if defined(ESP_PWR_LVL_P9)
  BLEDevice::setPower(ESP_PWR_LVL_P9);
#endif

  rcServer = BLEDevice::createServer();
  rcServer->setCallbacks(new RaceChronoServerCallbacks());

  BLEService *service = rcServer->createService(RC_BLE_SERVICE_UUID);

  rcCanMainCharacteristic = service->createCharacteristic(
      RC_BLE_CAN_MAIN_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  rcCanMainCharacteristic->addDescriptor(new BLE2902());

  rcCanFilterCharacteristic = service->createCharacteristic(
      RC_BLE_CAN_FILTER_UUID,
      BLECharacteristic::PROPERTY_WRITE);
  rcCanFilterCharacteristic->setCallbacks(new RaceChronoFilterCallbacks());

  service->start();

  rcClearFilters();
  rcAllowAll = true;
  rcDefaultNotifyMs = RC_DEFAULT_NOTIFY_MS;

  rcStartAdvertising();

  Serial.print("[BLE] Advertising name: ");
  Serial.println(RC_BLE_DEVICE_NAME);
}

static void rcSendCanFrame(uint32_t pid, const uint8_t *payload, uint8_t payloadLen) {
  if (!rcBleConnected || rcCanMainCharacteristic == nullptr) {
    return;
  }

  if (payloadLen > 16) {
    payloadLen = 16;
  }

  uint8_t frame[20];

  // Packet ID is little-endian per RaceChrono CAN-Bus main characteristic.
  frame[0] = (uint8_t)(pid & 0xFF);
  frame[1] = (uint8_t)((pid >> 8) & 0xFF);
  frame[2] = (uint8_t)((pid >> 16) & 0xFF);
  frame[3] = (uint8_t)((pid >> 24) & 0xFF);

  for (uint8_t i = 0; i < payloadLen; i++) {
    frame[4 + i] = payload[i];
  }

  rcCanMainCharacteristic->setValue(frame, 4 + payloadLen);
  rcCanMainCharacteristic->notify();
  rcNotifyCount++;
}

static void rcSendI16TempPid(uint32_t pid, float tempC, bool valid, uint32_t now) {
  if (!rcShouldSendPid(pid, now)) return;

  uint8_t payload[2];
  writeI16BE(payload, tempToI16x10(tempC, valid));
  rcSendCanFrame(pid, payload, sizeof(payload));
}

static void rcSendU16Pid(uint32_t pid, uint16_t value, uint32_t now) {
  if (!rcShouldSendPid(pid, now)) return;

  uint8_t payload[2];
  writeU16BE(payload, value);
  rcSendCanFrame(pid, payload, sizeof(payload));
}

static void rcSendU8Pid(uint32_t pid, uint8_t value, uint32_t now) {
  if (!rcShouldSendPid(pid, now)) return;

  uint8_t payload[1] = {value};
  rcSendCanFrame(pid, payload, sizeof(payload));
}

static void rcBleUpdate(uint32_t now) {
  if (!rcBleConnected) {
    return;
  }

  if ((now - rcLastSendMs) < RC_SEND_INTERVAL_MS) {
    return;
  }

  rcLastSendMs = now;

  bool anyOk = false;
  float displayT = getDisplayTemp(anyOk);

  rcSendI16TempPid(RC_PID_BRAKE_FL, sensors[0].tempC, sensors[0].enabled && sensors[0].ok, now);
  rcSendI16TempPid(RC_PID_BRAKE_FR, sensors[1].tempC, sensors[1].enabled && sensors[1].ok, now);
  rcSendI16TempPid(RC_PID_BRAKE_DISPLAY, displayT, anyOk, now);
  rcSendI16TempPid(RC_PID_BRAKE_FAST, tempFastC, !isnan(tempFastC), now);
  rcSendI16TempPid(RC_PID_BRAKE_CORE, tempCoreC, !isnan(tempCoreC), now);

  uint16_t dose10 = (uint16_t)clampInt((int)lroundf(thermalDose * 10.0f), 0, 65535);
  rcSendU16Pid(RC_PID_BRAKE_DOSE, dose10, now);
  rcSendU8Pid(RC_PID_BRAKE_STATE, (uint8_t)systemState, now);
  rcSendU16Pid(RC_PID_BRAKE_ALARM, (uint16_t)alarmTempC, now);
}

#endif

// ================================================================
// Button handling
// ================================================================

static void adjustAlarmTemp(int delta, uint32_t now) {
  int oldValue = alarmTempC;
  alarmTempC = clampInt(alarmTempC + delta, MIN_ALARM_TEMP_C, MAX_ALARM_TEMP_C);

  if (alarmTempC != oldValue) {
    settingsDirty = true;
    lastAdjustMs = now;
    savedNoticeUntilMs = 0;

    // ALM 调整后重置热模型，避免旧 dose 按旧阈值继续影响报警。
    thermalDose = 0.0f;
    thermalAlarmLatched = false;
    alarmLatched = false;
    coreOverAlarmSinceMs = 0;
    peakCriticalSinceMs = 0;
    peakWithSoakSinceMs = 0;

    shortBeep(12);

    Serial.print("[KEY] Alarm temp changed: ");
    Serial.print(alarmTempC);
    Serial.println(" C");
  }
}

static void handleButtons(uint32_t now) {
  btnSet.update(now);
  btnUp.update(now);
  btnDown.update(now);

  static uint32_t comboStartMs = 0;
  static bool comboTriggered = false;

  if (btnUp.pressed() && btnDown.pressed()) {
    if (comboStartMs == 0) {
      comboStartMs = now;
      comboTriggered = false;
    }

    if (!comboTriggered && (now - comboStartMs) >= 1000) {
      muteUntilMs = now + 60000UL;
      comboTriggered = true;
      shortBeep(60);
      Serial.println("[KEY] Buzzer muted for 60s");
    }
  } else {
    comboStartMs = 0;
    comboTriggered = false;
  }

  if (!(btnUp.pressed() && btnDown.pressed())) {
    if (btnUp.shortEvent || btnUp.repeatEvent) {
      adjustAlarmTemp(+ALARM_STEP_C, now);
    }

    if (btnDown.shortEvent || btnDown.repeatEvent) {
      adjustAlarmTemp(-ALARM_STEP_C, now);
    }
  }

  if (btnSet.shortEvent) {
    saveSettings();
    shortBeep(40);

    Serial.print("[KEY] Settings saved. ALM=");
    Serial.print(alarmTempC);
    Serial.print("C, buzzer=");
    Serial.println(buzzerEnabled ? "ON" : "OFF");
  }

  if (btnSet.longEvent) {
    buzzerEnabled = !buzzerEnabled;
    settingsDirty = true;
    saveSettings();
    buzzerNoticeUntilMs = now + 1500;

    Serial.print("[KEY] Buzzer toggled for FAIL/WARN: ");
    Serial.println(buzzerEnabled ? "ON" : "OFF");

    if (buzzerEnabled) {
      buzzerSet(true);
      delay(80);
      buzzerSet(false);
    }
  }

  if (settingsDirty && lastAdjustMs != 0 && (now - lastAdjustMs) > 3000) {
    saveSettings();

    Serial.print("[AUTO] Settings saved. ALM=");
    Serial.print(alarmTempC);
    Serial.println("C");
  }
}

// ================================================================
// Buzzer / LED
// ================================================================

static void updateBuzzer(uint32_t now) {
  bool on = false;

  bool inStartupGrace = (now - startupMs) < 3000;
  bool muted = (muteUntilMs != 0 && now < muteUntilMs);

  if (inStartupGrace || muted) {
    buzzerSet(false);
    return;
  }

  // B:OFF 只屏蔽 FAIL / WARN，不屏蔽真正超温 ALARM。
  if (!buzzerEnabled && systemState != STATE_ALARM) {
    buzzerSet(false);
    return;
  }

  switch (systemState) {
    case STATE_ALARM:
      on = ((now % 300) < 150);
      break;

    case STATE_WARN:
      on = ((now % 1500) < 80);
      break;

    case STATE_FAIL:
      on = false;
      break;

    case STATE_OK:
    default:
      on = false;
      break;
  }

  buzzerSet(on);
}

static void updateLed(uint32_t now) {
  bool on = false;

  switch (systemState) {
    case STATE_ALARM:
      on = ((now % 200) < 100);
      break;

    case STATE_WARN:
      on = ((now % 600) < 100);
      break;

    case STATE_FAIL:
      on = ((now % 1000) < 100);
      break;

    case STATE_OK:
    default:
      on = ((now % 1200) < 45);
      break;
  }

  ledSet(on);
}

// ================================================================
// OLED drawing - Large single temperature UI
// Boot animation block is intentionally kept identical to V1.4.
// Do not modify HYUNDAI_N_LOGO_XBM / drawBootAnimationFrame unless requested.
// ================================================================

static void drawBootScreen();

static void drawCenteredText(const char *text, const uint8_t *font, int y) {
  u8g2.setFont(font);
  int w = u8g2.getStrWidth(text);
  int x = (128 - w) / 2;
  if (x < 0) x = 0;
  u8g2.drawStr(x, y, text);
}

static const char *sensorOnlineText(const BrakeSensor &s) {
  if (!s.enabled) return "DIS";
  return s.ok ? "ON" : "--";
}

static void drawSensorStatusBar() {
  char status[24];
#if ENABLE_RACECHRONO_BLE
  snprintf(status, sizeof(status), "FL:%s FR:%s BLE:%s",
           sensorOnlineText(sensors[0]),
           sensorOnlineText(sensors[1]),
           rcBleConnected ? "ON" : "--");
#else
  snprintf(status, sizeof(status), "FL:%s FR:%s",
           sensorOnlineText(sensors[0]),
           sensorOnlineText(sensors[1]));
#endif

  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(3, 8, status);
}

static int lerpByTime(uint32_t t, uint32_t t0, uint32_t t1, int v0, int v1) {
  if (t <= t0) return v0;
  if (t >= t1) return v1;

  long span = (long)(t1 - t0);
  long pos = (long)(t - t0);
  return v0 + (int)((pos * (long)(v1 - v0)) / span);
}

static void drawFilledQuad(int x0, int y0,
                           int x1, int y1,
                           int x2, int y2,
                           int x3, int y3) {
  u8g2.drawTriangle(x0, y0, x1, y1, x2, y2);
  u8g2.drawTriangle(x0, y0, x2, y2, x3, y3);
}

static void drawHyundaiNMark(int x, int y, int s) {
  (void)s;
  u8g2.drawXBMP(x, y, HYUNDAI_N_LOGO_W, HYUNDAI_N_LOGO_H, HYUNDAI_N_LOGO_XBM);
}

static void drawHyundaiWordReveal(uint8_t count, int x, int y) {
  const char *text = "HYUNDAI";
  char buf[8];

  if (count > 7) count = 7;

  for (uint8_t i = 0; i < count; i++) {
    buf[i] = text[i];
  }

  buf[count] = '\0';
  u8g2.setFont(u8g2_font_7x13B_tf);
  u8g2.drawStr(x, y, buf);
}

static int hyundaiWordRevealWidth(uint8_t count) {
  const char *text = "HYUNDAI";
  char buf[8];

  if (count > 7) count = 7;

  for (uint8_t i = 0; i < count; i++) {
    buf[i] = text[i];
  }

  buf[count] = '\0';
  u8g2.setFont(u8g2_font_7x13B_tf);
  return u8g2.getStrWidth(buf);
}

static void drawHyundaiNLockup(uint8_t chars, int nX) {
  drawHyundaiWordReveal(chars, 9, 37);
  drawHyundaiNMark(nX, 18, 1);
}

static void drawBootAnimationFrame(uint32_t elapsed) {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);

  if (elapsed < 420) {
    int dotR = lerpByTime(elapsed, 0, 420, 0, 2);
    u8g2.drawDisc(64, 32, dotR);
  } else if (elapsed < 1450) {
    int ringR = lerpByTime(elapsed, 420, 1100, 2, 26);

    u8g2.drawCircle(64, 32, ringR);
    if (ringR > 3) u8g2.drawCircle(64, 32, ringR - 1);
    if ((elapsed / 130) % 2 == 0 && ringR > 16) {
      u8g2.drawCircle(64, 32, ringR + 2);
    }

    if (elapsed > 780) {
      drawHyundaiNMark(32, 18, 1);
    }
  } else {
    uint8_t chars = (uint8_t)lerpByTime(elapsed, 1650, 2520, 0, 7);
    int nX = lerpByTime(elapsed, 1450, 2050, 32, 63);

    drawHyundaiNLockup(chars, nX);

    if (chars < 7) {
      int cursorX = 9 + hyundaiWordRevealWidth(chars);
      u8g2.drawVLine(cursorX, 25, 14);
    }
  }

  u8g2.sendBuffer();
}

static void playBootAnimation() {
  if (!BOOT_ANIMATION_ENABLE) {
    drawBootScreen();
    delay(500);
    return;
  }

  uint32_t start = millis();
  uint32_t elapsed = 0;

  while (elapsed < BOOT_ANIMATION_MS) {
    drawBootAnimationFrame(elapsed);
    delay(BOOT_FRAME_MS);
    elapsed = millis() - start;
  }

  drawBootAnimationFrame(BOOT_ANIMATION_MS);
  delay(250);
}

static void drawLargeTemp(uint32_t now) {
  char tempBuf[16];
  bool anyOk = false;
  float t = getDisplayTemp(anyOk);

  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);

  // 报警时做整屏边框闪烁，不显示 ALARM 文字，避免占空间。
  if (systemState == STATE_ALARM && ((now / 250) % 2 == 0)) {
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawFrame(1, 1, 126, 62);
  } else if (systemState == STATE_WARN) {
    u8g2.drawFrame(0, 0, 128, 64);
  }

  drawSensorStatusBar();

  if (anyOk) {
    // 赛道场景下温度通常三位数，为了最大化字体，只显示整数温度。
    int tempInt = (int)roundf(t);
    tempInt = clampInt(tempInt, -99, 999);

    snprintf(tempBuf, sizeof(tempBuf), "%d", tempInt);

    u8g2.setFont(u8g2_font_logisoso42_tn);
    int numW = u8g2.getStrWidth(tempBuf);

    int x = (128 - numW - 13) / 2;
    if (x < 0) x = 0;

    u8g2.drawStr(x, 50, tempBuf);

    u8g2.setFont(u8g2_font_9x15B_tf);
    u8g2.drawStr(x + numW + 3, 47, "C");
  } else {
    drawCenteredText("----", u8g2_font_logisoso32_tn, 45);
  }

  // 底部保留 ALM 和设置状态。用小字显示热负荷，便于路试标定。
  char bottom[28];

  if (!isnan(tempCoreC)) {
    snprintf(bottom, sizeof(bottom), "ALM:%d D:%02d", alarmTempC, (int)thermalDose);
  } else {
    snprintf(bottom, sizeof(bottom), "ALM:%d", alarmTempC);
  }

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 63, bottom);

  if (savedNoticeUntilMs != 0 && now < savedNoticeUntilMs) {
    u8g2.drawStr(88, 63, "SAVED");
  } else if (buzzerNoticeUntilMs != 0 && now < buzzerNoticeUntilMs) {
    u8g2.drawStr(92, 63, buzzerEnabled ? "B:ON" : "B:OFF");
  } else if (muteUntilMs != 0 && now < muteUntilMs) {
    u8g2.drawStr(96, 63, "MUTE");
  } else if (!buzzerEnabled) {
    u8g2.drawStr(92, 63, "B:OFF");
  } else if (settingsDirty) {
    u8g2.drawStr(100, 63, "*");
  }

  u8g2.sendBuffer();
}

static void updateDisplay(uint32_t now) {
  if ((now - lastDisplayMs) < DISPLAY_REFRESH_MS) {
    return;
  }

  lastDisplayMs = now;
  drawLargeTemp(now);
}

static void drawBootScreen() {
  u8g2.clearBuffer();
  u8g2.setFontMode(1);
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(4, 12, "Brake Temp Node");

  u8g2.setFont(u8g2_font_9x15B_tf);
  u8g2.drawStr(10, 36, "12V RS485");

  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.drawStr(5, 55, "Thermal BLE V1.5");

  u8g2.sendBuffer();
}

// ================================================================
// Debug
// ================================================================

static void printDebug(uint32_t now) {
  if ((now - lastDebugMs) < DEBUG_PRINT_MS) {
    return;
  }

  lastDebugMs = now;

  Serial.print("[DATA] ");

  for (int i = 0; i < 2; i++) {
    Serial.print(sensors[i].name);
    Serial.print("=");

    if (sensors[i].enabled && sensors[i].ok && !isnan(sensors[i].tempC)) {
      Serial.print(sensors[i].tempC, 1);
      Serial.print("C");
    } else if (!sensors[i].enabled) {
      Serial.print("DIS");
    } else {
      Serial.print("FAIL");
    }

    Serial.print(" raw=");
    Serial.print(sensors[i].lastRaw);

    Serial.print(" cand=");
    if (sensors[i].preferredCandidate >= 0 &&
        sensors[i].preferredCandidate < MODBUS_CANDIDATE_COUNT) {
      Serial.print(MODBUS_CANDIDATES[sensors[i].preferredCandidate].name);
    } else {
      Serial.print("--");
    }

    Serial.print("  ");
  }

  bool anyOk = false;
  float shownTemp = getDisplayTemp(anyOk);

  Serial.print("DISPLAY=");
  if (anyOk) {
    Serial.print(shownTemp, 1);
    Serial.print("C");
  } else {
    Serial.print("----");
  }

  Serial.print(" Tfast=");
  if (!isnan(tempFastC)) Serial.print(tempFastC, 1);
  else Serial.print("nan");

  Serial.print(" Tcore=");
  if (!isnan(tempCoreC)) Serial.print(tempCoreC, 1);
  else Serial.print("nan");

  Serial.print(" Dose=");
  Serial.print(thermalDose, 1);

  Serial.print(" STATE=");
  Serial.print(stateText(systemState));

  Serial.print(" ALM=");
  Serial.print(alarmTempC);
  Serial.print("C");

  Serial.print(" BUZZ=");
  Serial.print(buzzerEnabled ? "ON" : "OFF");

#if ENABLE_RACECHRONO_BLE
  Serial.print(" BLE=");
  Serial.print(rcBleConnected ? "ON" : "--");
  Serial.print(" N=");
  Serial.print(rcNotifyCount);
#endif

  if (settingsDirty) {
    Serial.print(" DIRTY");
  }

  if (muteUntilMs != 0 && now < muteUntilMs) {
    Serial.print(" MUTE");
  }

  Serial.println();
}

// ================================================================
// Setup / Loop
// ================================================================

void setup() {
  startupMs = millis();

  pinMode(PIN_RS485_DE, OUTPUT);
  rs485ReceiveMode();

  pinMode(PIN_BUZZER_CTRL, OUTPUT);
  buzzerSet(false);

  pinMode(PIN_LED_STAT, OUTPUT);
  ledSet(false);

  btnSet.begin(PIN_KEY_SET);
  btnUp.begin(PIN_KEY_UP);
  btnDown.begin(PIN_KEY_DOWN);

  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("==================================================");
  Serial.println("Brake Temp Node 12V RS485 Alarm Display V1.5");
  Serial.println("Hyundai N OLED UI + Thermal Load Alarm + RaceChrono BLE");
  Serial.println("ESP32-C3 + SP3485 + SH1106 OLED + Buzzer + Keys");
  Serial.println("Default protocol: Modbus RTU, 9600, 8N1");
  Serial.println("FL addr=0x01, FR addr=0x02");
  Serial.println("RaceChrono BLE DIY CAN enabled");
  Serial.println("==================================================");
  Serial.print("PIN_RS485_DE    = GPIO"); Serial.println(PIN_RS485_DE);
  Serial.print("PIN_OLED_SCL    = GPIO"); Serial.println(PIN_OLED_SCL);
  Serial.print("PIN_OLED_SDA    = GPIO"); Serial.println(PIN_OLED_SDA);
  Serial.print("PIN_BUZZER_CTRL = GPIO"); Serial.println(PIN_BUZZER_CTRL);
  Serial.print("PIN_KEY_SET     = GPIO"); Serial.println(PIN_KEY_SET);
  Serial.print("PIN_KEY_UP      = GPIO"); Serial.println(PIN_KEY_UP);
  Serial.print("PIN_KEY_DOWN    = GPIO"); Serial.println(PIN_KEY_DOWN);
  Serial.print("PIN_LED_STAT    = GPIO"); Serial.println(PIN_LED_STAT);
  Serial.print("PIN_RS485_RX    = GPIO"); Serial.println(PIN_RS485_RX);
  Serial.print("PIN_RS485_TX    = GPIO"); Serial.println(PIN_RS485_TX);
  Serial.println("==================================================");

  loadSettings();

  Serial.print("[SET] Alarm threshold = ");
  Serial.print(alarmTempC);
  Serial.println(" C");

  Serial.print("[SET] Buzzer for FAIL/WARN = ");
  Serial.println(buzzerEnabled ? "ON" : "OFF");

  Wire.begin(PIN_OLED_SDA, PIN_OLED_SCL);
  Wire.setClock(100000);

  u8g2.setI2CAddress(OLED_I2C_ADDR << 1);
  u8g2.begin();
  u8g2.setPowerSave(0);

  playBootAnimation();

  RS485Bus.begin(RS485_BAUD,
                 RS485_SERIAL_CONFIG,
                 PIN_RS485_RX,
                 PIN_RS485_TX);

  rs485ReceiveMode();

#if ENABLE_RACECHRONO_BLE
  rcBleBegin();
#endif

  ledSet(true);
  shortBeep(60);
  delay(80);
  ledSet(false);
  shortBeep(40);

  delay(500);
  startupMs = millis();
}

void loop() {
  uint32_t now = millis();

  handleButtons(now);
  pollSensors(now);
  updateThermalModel(now);
  updateAlarmState(now);
  updateBuzzer(now);
  updateLed(now);
  updateDisplay(now);
#if ENABLE_RACECHRONO_BLE
  rcBleUpdate(now);
#endif
  printDebug(now);
}
