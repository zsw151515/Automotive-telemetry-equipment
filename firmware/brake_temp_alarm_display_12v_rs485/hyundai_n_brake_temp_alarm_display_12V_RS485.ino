/*
  Brake Temp Node 12V RS485 Alarm Display
  Board: break_temp_alarm_display_12V_RS485
  MCU: ESP32-C3
  Display: HS13L03W2C01 / SH1106 128x64 I2C OLED
  RS485: SP3485, Modbus RTU master

  Version: V1.4 Hyundai N OLED Boot Animation
  Date: 2026-05-26

  Display V1.4:
    - No top title
    - No FL / FR label
    - No FAIL text in header
    - No MAX label
    - Large single temperature number only
    - Top sensor online status
    - ALM display retained
    - SET / UP / DOWN logic retained
    - Hyundai N style startup animation retained in monochrome OLED form
    - FAIL / sensor disconnected buzzer beep disabled

  Key behavior:
    UP short / hold      -> ALM +10C
    DOWN short / hold    -> ALM -10C
    SET short            -> Save ALM to NVS
    SET long             -> Toggle buzzer enable
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

  Notes:
    - LED_STAT is active LOW.
    - BUZZER_CTRL is active HIGH.
    - Keys are active LOW.
    - RS485_DE HIGH = transmit, LOW = receive.
*/

#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <math.h>

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
#define ALARM_HYSTERESIS_C 30
#define WARN_MARGIN_C 50

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
      delay(1);
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

  for (int i = 0; i < MODBUS_CANDIDATE_COUNT; i++) {
    if (i == s.preferredCandidate) continue;

    if (tryReadTempWithCandidate(s.addr, MODBUS_CANDIDATES[i], tempCOut, rawOut)) {
      s.preferredCandidate = i;
      return true;
    }
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
    s.tempC = t;
    s.lastRaw = raw;
    s.ok = true;
    s.lastOkMs = now;
    s.failCount = 0;
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
// Alarm logic
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

static void updateAlarmState(uint32_t now) {
  bool anyOk = false;
  float displayT = getDisplayTemp(anyOk);
  bool anyFail = hasAnySensorFail();

  if (!anyOk) {
    alarmLatched = false;
    systemState = STATE_FAIL;
    return;
  }

  if (!alarmLatched && displayT >= (float)alarmTempC) {
    alarmLatched = true;
  }

  if (alarmLatched && displayT <= (float)(alarmTempC - ALARM_HYSTERESIS_C)) {
    alarmLatched = false;
    muteUntilMs = 0;
  }

  if (alarmLatched) {
    systemState = STATE_ALARM;
  } else if (displayT >= (float)(alarmTempC - WARN_MARGIN_C)) {
    systemState = STATE_WARN;
  } else if (anyFail) {
    systemState = STATE_FAIL;
  } else {
    systemState = STATE_OK;
  }
}

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

    Serial.print("[KEY] Buzzer toggled: ");
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

  if (!buzzerEnabled || inStartupGrace || muted) {
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
  snprintf(status, sizeof(status), "FL:%s FR:%s",
           sensorOnlineText(sensors[0]),
           sensorOnlineText(sensors[1]));

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

    // 大数字
    u8g2.setFont(u8g2_font_logisoso42_tn);
    int numW = u8g2.getStrWidth(tempBuf);

    int x = (128 - numW - 13) / 2;
    if (x < 0) x = 0;

    u8g2.drawStr(x, 50, tempBuf);

    // 小 C
    u8g2.setFont(u8g2_font_9x15B_tf);
    u8g2.drawStr(x + numW + 3, 47, "C");
  } else {
    // 不显示 FAIL，只显示 ----
    drawCenteredText("----", u8g2_font_logisoso32_tn, 45);
  }

  // 底部保留 ALM 和设置状态
  char bottom[24];
  snprintf(bottom, sizeof(bottom), "ALM:%d", alarmTempC);

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
  u8g2.drawStr(5, 55, "Hyundai N Display V1.4");

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

  Serial.print(" STATE=");
  Serial.print(stateText(systemState));

  Serial.print(" ALM=");
  Serial.print(alarmTempC);
  Serial.print("C");

  Serial.print(" BUZZ=");
  Serial.print(buzzerEnabled ? "ON" : "OFF");

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
  Serial.println("Brake Temp Node 12V RS485 Alarm Display V1.4");
  Serial.println("Hyundai N OLED UI + Boot Animation");
  Serial.println("ESP32-C3 + SP3485 + SH1106 OLED + Buzzer + Keys");
  Serial.println("Default protocol: Modbus RTU, 9600, 8N1");
  Serial.println("FL addr=0x01, FR addr=0x02");
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

  Serial.print("[SET] Buzzer = ");
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
  updateAlarmState(now);
  updateBuzzer(now);
  updateLed(now);
  updateDisplay(now);
  printDebug(now);
}
