#include <Wire.h>
#include <Adafruit_MLX90640.h>
#include <VL53L1X.h>

#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include <math.h>

// =====================================================
// ESP32-C3 + MLX90640 + VL53L1X
// RaceChrono / RejsaRubberTrac compatible BLE only
// =====================================================

// RaceChrono recognizes the RejsaRubberTrac-style BLE layout by name/service.
const char* DEVICE_NAME = "RejsaRubberRL123456";

// 1=FL, 2=FR, 3=RL, 4=RR
const uint8_t TIRE_ID = 3;

// V3 board pins.
const int SDA_PIN          = 8;
const int SCL_PIN          = 10;
const int VBAT_ADC_PIN     = 1;
const int LED_STAT_PIN     = 2;  // Active low.
const int BTN_PIN          = 3;  // Active low.
const int PWR_KILL_PIN     = 4;  // HIGH keeps power on.
const int PWR_INT_PIN      = 5;
const int VL53_XSHUT_PIN   = 6;
const int VL53_INT_PIN     = 7;

// Button behavior:
// single click = height zero, double click = mirror zones, long press = power off.
const bool ENABLE_BUTTON_CTRL = true;
const bool AUTO_MIRROR_BY_TIRE_ID = true;
bool mirrorZones = false;

// Fast profile is the preferred "most responsive" mode. If the bus proves
// unstable, firmware falls back to the stable profile automatically.
const bool START_FAST_PROFILE = true;
const uint32_t I2C_FAST_CLOCK = 1000000;
const uint32_t I2C_STABLE_CLOCK = 400000;
const uint32_t FRAME_FAST_INTERVAL_MS = 34;    // For MLX 32 Hz.
const uint32_t FRAME_STABLE_INTERVAL_MS = 70;  // For MLX 16 Hz.
const uint32_t SEND_INTERVAL_MS = 30;          // About 33 Hz BLE notifications.
const uint32_t TOF_INTERVAL_MS = 50;           // 20 Hz height.
const uint32_t BATTERY_INTERVAL_MS = 1000;

// MLX frame and ROI.
const int FRAME_ROWS = 24;
const int FRAME_COLS = 32;
const int ROI_HEIGHT = 8;
const int ROI_SEARCH_MIN_ROW = 3;
const int ROI_SEARCH_MAX_ROW = 17;
const int ZONE_COUNT = 8;
const int ZONE_COLS = FRAME_COLS / ZONE_COUNT;
const int ZONE_PIXELS = ROI_HEIGHT * ZONE_COLS;

// Temperature cleaning. The order is:
// valid-range reject -> trimmed mean per zone -> rate limit -> adaptive low-pass.
const float TEMP_MIN_C = -20.0f;
const float TEMP_MAX_C = 200.0f;
const int MIN_VALID_PIXELS_PER_ZONE = 18;
const uint8_t TRIM_PIXELS_PER_ZONE_SIDE = 3;
const uint8_t TRIM_PIXELS_PER_ROW_SIDE = 3;

const float ROI_TRACK_ALPHA = 0.35f;
const int MAX_ROI_STEP_PER_FRAME = 1;

const float TEMP_DEADBAND_C = 0.06f;
const float TEMP_MAX_RATE_C_PER_S = 160.0f;
const float RAW_SPIKE_GATE_C = 12.0f;
const uint32_t TEMP_STALE_MS = 1500;
const int WARMUP_GOOD_FRAMES = 2;

const int MAX_BAD_FRAME_STREAK = 8;
const int MLX_RECOVER_BAD_STREAK = 12;
const uint8_t FAST_RECOVERIES_BEFORE_STABLE = 2;
const uint32_t MLX_ERR_LOG_INTERVAL_MS = 1000;

// VL53L1X.
const bool TOF_USE_SHORT_MODE = true;
const float TOF_FILTER_ALPHA = 0.30f;
const uint32_t TOF_BASELINE_TIME_MS = 2000;
const int TOF_VALID_MIN_MM = 20;
const int TOF_VALID_MAX_MM = 1000;

// Battery.
const float ADC_REF_VOLT = 3.3f;
const int ADC_MAX = 4095;
const float VBAT_DIVIDER = 2.0f;

// Button.
const uint32_t BTN_DEBOUNCE_MS = 8;
const uint32_t BTN_DOUBLE_CLICK_MS = 650;
const uint32_t BTN_LONG_PRESS_SHUTDOWN_MS = 1800;
const uint32_t STARTUP_IGNORE_BUTTON_MS = 1200;

// =====================================================
// Sensors / BLE state
// =====================================================
Adafruit_MLX90640 mlx;
VL53L1X tof;

float frameBuf[FRAME_ROWS * FRAME_COLS];
float zoneFiltered[ZONE_COUNT] = {0};
float zoneLastRaw[ZONE_COUNT] = {0};
int16_t zoneOut10[ZONE_COUNT] = {0};  // 0.1 C, sent to RaceChrono.

bool mlxFastProfile = START_FAST_PROFILE;
bool tempFilterInited = false;
bool tempEverValid = false;
int goodFrameCount = 0;
int badFrameStreak = 0;
uint32_t lastGoodFrameMs = 0;
uint32_t lastMlxErrLogMs = 0;
uint32_t mlxRecoverCount = 0;

int roiRowStart = 8;
float roiRowStartFiltered = 8.0f;

bool tofReady = false;
int tofDistanceRawMm = -1;
float tofDistanceFilteredMm = -1.0f;
int tofTravelMm = 0;
bool tofBaselineReady = false;
int tofBaselineMm = 0;
uint32_t tofBaselineStartMs = 0;
long tofBaselineAccum = 0;
int tofBaselineCount = 0;

uint16_t batteryMv = 0;
uint8_t batteryPct = 0;

BLEServer* bleServer = nullptr;
BLECharacteristic* bleChar1 = nullptr;
BLECharacteristic* bleChar2 = nullptr;
BLECharacteristic* bleChar3 = nullptr;
volatile bool bleConnected = false;
uint32_t bleConnectedAtMs = 0;

uint32_t bootMs = 0;
uint32_t lastFrameMs = 0;
uint32_t lastSendMs = 0;
uint32_t lastTofMs = 0;
uint32_t lastBatteryMs = 0;
uint32_t lastDebugMs = 0;

// =====================================================
// Button / LED state
// =====================================================
bool btnStableState = HIGH;
bool btnPressed = false;
bool btnLongHandled = false;
bool btnWaitingSecondClick = false;
bool ignoreButtonUntilReleased = false;
uint32_t btnPressStartMs = 0;
uint32_t btnFirstClickReleasedMs = 0;
uint32_t btnLastAcceptedEdgeMs = 0;

const uint8_t BTN_IRQ_Q_SIZE = 16;
volatile uint8_t btnIrqHead = 0;
volatile uint8_t btnIrqTail = 0;
volatile uint8_t btnIrqLevel[BTN_IRQ_Q_SIZE];
volatile uint32_t btnIrqMs[BTN_IRQ_Q_SIZE];
portMUX_TYPE btnIrqMux = portMUX_INITIALIZER_UNLOCKED;

int ledPatternTogglesLeft = 0;
bool ledPatternState = false;
uint32_t ledPatternNextMs = 0;
uint16_t ledPatternOnMs = 80;
uint16_t ledPatternOffMs = 80;
bool ledHeartbeatState = false;
uint32_t lastLedHeartbeatMs = 0;

// =====================================================
// Small helpers
// =====================================================
void putI16LE(uint8_t* buf, int idx, int16_t value) {
  buf[idx] = (uint8_t)(value & 0xFF);
  buf[idx + 1] = (uint8_t)((value >> 8) & 0xFF);
}

void putU16LE(uint8_t* buf, int idx, uint16_t value) {
  buf[idx] = (uint8_t)(value & 0xFF);
  buf[idx + 1] = (uint8_t)((value >> 8) & 0xFF);
}

int clampInt(int value, int lo, int hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

float clampFloat(float value, float lo, float hi) {
  if (value < lo) return lo;
  if (value > hi) return hi;
  return value;
}

float limitStep(float current, float target, float maxStep) {
  float d = target - current;
  if (d > maxStep) return current + maxStep;
  if (d < -maxStep) return current - maxStep;
  return target;
}

bool validTemp(float t) {
  return !isnan(t) && !isinf(t) && t >= TEMP_MIN_C && t <= TEMP_MAX_C;
}

void insertionSort(float* values, int count) {
  for (int i = 1; i < count; i++) {
    float x = values[i];
    int j = i - 1;
    while (j >= 0 && values[j] > x) {
      values[j + 1] = values[j];
      j--;
    }
    values[j + 1] = x;
  }
}

float trimmedMean(float* values, int count, uint8_t trimEachSide) {
  if (count <= 0) return NAN;
  insertionSort(values, count);

  int trim = trimEachSide;
  if (count <= trim * 2 + 2) trim = 0;

  float sum = 0.0f;
  int used = 0;
  for (int i = trim; i < count - trim; i++) {
    sum += values[i];
    used++;
  }

  return used > 0 ? (sum / used) : NAN;
}

float tempAlphaForDelta(float deltaC) {
  if (deltaC < 0.15f) return 0.10f;
  if (deltaC < 0.70f) return 0.24f;
  if (deltaC < 2.00f) return 0.45f;
  return 0.72f;
}

uint32_t frameIntervalMs() {
  return mlxFastProfile ? FRAME_FAST_INTERVAL_MS : FRAME_STABLE_INTERVAL_MS;
}

uint32_t mlxClock() {
  return mlxFastProfile ? I2C_FAST_CLOCK : I2C_STABLE_CLOCK;
}

// =====================================================
// LED / power
// =====================================================
void setStatusLed(bool on) {
  digitalWrite(LED_STAT_PIN, on ? LOW : HIGH);
}

void startLedFlash(uint8_t flashes, uint16_t onMs, uint16_t offMs) {
  ledPatternTogglesLeft = flashes * 2;
  ledPatternOnMs = onMs;
  ledPatternOffMs = offMs;
  ledPatternState = true;
  setStatusLed(true);
  ledPatternNextMs = millis() + ledPatternOnMs;
}

void updateStatusLed() {
  uint32_t now = millis();

  if (ledPatternTogglesLeft > 0) {
    if (now >= ledPatternNextMs) {
      ledPatternState = !ledPatternState;
      setStatusLed(ledPatternState);
      ledPatternTogglesLeft--;
      ledPatternNextMs = now + (ledPatternState ? ledPatternOnMs : ledPatternOffMs);
    }
    return;
  }

  uint32_t period = bleConnected ? 1000 : 250;
  if (now - lastLedHeartbeatMs >= period) {
    lastLedHeartbeatMs = now;
    ledHeartbeatState = !ledHeartbeatState;
    setStatusLed(ledHeartbeatState);
  }
}

void powerOffNow(const char* reason) {
  static bool shutdownStarted = false;
  if (shutdownStarted) {
    while (true) {
      digitalWrite(PWR_KILL_PIN, LOW);
      delay(100);
    }
  }
  shutdownStarted = true;

  Serial.print("Power off: ");
  Serial.println(reason);
  startLedFlash(1, 160, 80);

  uint32_t t0 = millis();
  while (millis() - t0 < 260) {
    updateStatusLed();
    delay(5);
  }

  detachInterrupt(digitalPinToInterrupt(BTN_PIN));
  digitalWrite(PWR_KILL_PIN, LOW);
  setStatusLed(false);

  while (true) {
    digitalWrite(PWR_KILL_PIN, LOW);
    delay(100);
  }
}

// =====================================================
// Button
// =====================================================
void IRAM_ATTR onBtnChangeISR() {
  uint8_t level = (uint8_t)digitalRead(BTN_PIN);
  uint32_t t = millis();

  portENTER_CRITICAL_ISR(&btnIrqMux);
  uint8_t next = (uint8_t)((btnIrqHead + 1) & (BTN_IRQ_Q_SIZE - 1));
  if (next != btnIrqTail) {
    btnIrqLevel[btnIrqHead] = level;
    btnIrqMs[btnIrqHead] = t;
    btnIrqHead = next;
  }
  portEXIT_CRITICAL_ISR(&btnIrqMux);
}

bool popBtnIrqEvent(uint8_t& level, uint32_t& eventMs) {
  bool ok = false;

  portENTER_CRITICAL(&btnIrqMux);
  if (btnIrqTail != btnIrqHead) {
    level = btnIrqLevel[btnIrqTail];
    eventMs = btnIrqMs[btnIrqTail];
    btnIrqTail = (uint8_t)((btnIrqTail + 1) & (BTN_IRQ_Q_SIZE - 1));
    ok = true;
  }
  portEXIT_CRITICAL(&btnIrqMux);

  return ok;
}

bool defaultMirrorByTireId(uint8_t id) {
  switch (id) {
    case 1: return false;
    case 2: return true;
    case 3: return false;
    case 4: return true;
    default: return false;
  }
}

bool calibrateTravelBaseline() {
  if (!tofReady) {
    Serial.println("Height zero skipped: VL53L1X not ready");
    return false;
  }

  int currentMm = tofDistanceRawMm;
  if (tofDistanceFilteredMm > 0.0f) currentMm = (int)(tofDistanceFilteredMm + 0.5f);

  if (currentMm < TOF_VALID_MIN_MM || currentMm > TOF_VALID_MAX_MM) {
    Serial.println("Height zero skipped: invalid distance");
    return false;
  }

  tofBaselineMm = currentMm;
  tofBaselineReady = true;
  tofTravelMm = 0;

  Serial.print("Height zero = ");
  Serial.print(tofBaselineMm);
  Serial.println(" mm");
  startLedFlash(2, 220, 180);
  return true;
}

void toggleMirrorZones() {
  mirrorZones = !mirrorZones;
  Serial.print("Mirror = ");
  Serial.println(mirrorZones ? "ON" : "OFF");
  startLedFlash(2, 80, 80);
}

void registerQuickClick(uint32_t eventMs) {
  if (!btnWaitingSecondClick) {
    btnWaitingSecondClick = true;
    btnFirstClickReleasedMs = eventMs;
    return;
  }

  if (eventMs - btnFirstClickReleasedMs <= BTN_DOUBLE_CLICK_MS) {
    btnWaitingSecondClick = false;
    toggleMirrorZones();
  } else {
    btnFirstClickReleasedMs = eventMs;
  }
}

void setupButtonAndMirror() {
  mirrorZones = AUTO_MIRROR_BY_TIRE_ID ? defaultMirrorByTireId(TIRE_ID) : mirrorZones;

  if (!ENABLE_BUTTON_CTRL) return;

  pinMode(BTN_PIN, INPUT_PULLUP);
  btnStableState = digitalRead(BTN_PIN);
  btnLastAcceptedEdgeMs = millis();

  portENTER_CRITICAL(&btnIrqMux);
  btnIrqHead = 0;
  btnIrqTail = 0;
  portEXIT_CRITICAL(&btnIrqMux);

  attachInterrupt(digitalPinToInterrupt(BTN_PIN), onBtnChangeISR, CHANGE);

  ignoreButtonUntilReleased = (btnStableState == LOW);
  if (ignoreButtonUntilReleased) {
    Serial.println("Button held at boot; waiting for release");
  }
}

void handleButton() {
  if (!ENABLE_BUTTON_CTRL) return;

  uint32_t now = millis();
  uint8_t level = HIGH;
  uint32_t eventMs = 0;

  while (popBtnIrqEvent(level, eventMs)) {
    if (eventMs - btnLastAcceptedEdgeMs < BTN_DEBOUNCE_MS) continue;
    btnLastAcceptedEdgeMs = eventMs;
    if (level == btnStableState) continue;

    btnStableState = level;

    if (ignoreButtonUntilReleased) {
      if (btnStableState == HIGH && eventMs - bootMs > STARTUP_IGNORE_BUTTON_MS) {
        ignoreButtonUntilReleased = false;
        btnPressed = false;
        btnLongHandled = false;
        Serial.println("Button normal logic enabled");
      }
      continue;
    }

    if (btnStableState == LOW) {
      btnPressed = true;
      btnLongHandled = false;
      btnPressStartMs = eventMs;
    } else {
      if (btnPressed && !btnLongHandled) registerQuickClick(eventMs);
      btnPressed = false;
      btnLongHandled = false;
    }
  }

  if (ignoreButtonUntilReleased) {
    if (digitalRead(BTN_PIN) == HIGH && now - bootMs > STARTUP_IGNORE_BUTTON_MS) {
      ignoreButtonUntilReleased = false;
      btnStableState = HIGH;
      btnPressed = false;
      btnLongHandled = false;
      Serial.println("Button normal logic enabled");
    }
    return;
  }

  if (btnPressed && !btnLongHandled && btnStableState == LOW) {
    if (now - btnPressStartMs >= BTN_LONG_PRESS_SHUTDOWN_MS) {
      btnLongHandled = true;
      powerOffNow("button long press");
    }
  }

  if (btnWaitingSecondClick && now - btnFirstClickReleasedMs > BTN_DOUBLE_CLICK_MS) {
    btnWaitingSecondClick = false;
    calibrateTravelBaseline();
  }
}

// =====================================================
// Battery
// =====================================================
void updateBattery(bool force) {
  uint32_t now = millis();
  if (!force && now - lastBatteryMs < BATTERY_INTERVAL_MS) return;
  lastBatteryMs = now;

  uint32_t sum = 0;
  const uint8_t samples = 8;
  for (uint8_t i = 0; i < samples; i++) {
    sum += analogRead(VBAT_ADC_PIN);
  }

  float raw = (float)sum / samples;
  float vAdc = raw * ADC_REF_VOLT / ADC_MAX;
  float vBat = vAdc * VBAT_DIVIDER;
  batteryMv = (uint16_t)(vBat * 1000.0f + 0.5f);

  if (batteryMv >= 4200) batteryPct = 100;
  else if (batteryMv <= 3300) batteryPct = 0;
  else batteryPct = (uint8_t)(((long)batteryMv - 3300L) * 100L / 900L);
}

// =====================================================
// BLE / RejsaRubberTrac packets
// =====================================================
void buildPacket1(uint8_t* buf) {
  memset(buf, 0, 20);
  buf[0] = 0x02;
  putI16LE(buf, 2, (int16_t)tofTravelMm);
  for (int i = 0; i < ZONE_COUNT; i++) putI16LE(buf, 4 + i * 2, zoneOut10[i]);
}

void buildPacket2(uint8_t* buf) {
  memset(buf, 0, 20);
  buf[0] = 0x02;
  buf[1] = batteryPct;
  putU16LE(buf, 2, batteryMv);
  for (int i = 0; i < ZONE_COUNT; i++) putI16LE(buf, 4 + i * 2, zoneOut10[i]);
}

void buildPacket3(uint8_t* buf) {
  memset(buf, 0, 20);
  buf[0] = 0x02;
  putI16LE(buf, 2, (int16_t)tofTravelMm);
  for (int i = 0; i < ZONE_COUNT; i++) putI16LE(buf, 4 + i * 2, zoneOut10[i]);
}

class ServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* server) override {
    bleConnected = true;
    bleConnectedAtMs = millis();
    Serial.println("BLE connected");
  }

  void onDisconnect(BLEServer* server) override {
    bleConnected = false;
    Serial.println("BLE disconnected");
    delay(200);
    BLEDevice::startAdvertising();
    Serial.println("BLE advertising restarted");
  }
};

void setupBLE() {
  BLEDevice::init(DEVICE_NAME);
  BLEDevice::setPower(ESP_PWR_LVL_P9);

  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  BLEService* service = bleServer->createService(BLEUUID((uint16_t)0x1FF7));

  bleChar1 = service->createCharacteristic(
      BLEUUID((uint16_t)0x0001),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleChar1->addDescriptor(new BLE2902());

  bleChar2 = service->createCharacteristic(
      BLEUUID((uint16_t)0x0002),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleChar2->addDescriptor(new BLE2902());

  bleChar3 = service->createCharacteristic(
      BLEUUID((uint16_t)0x0003),
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  bleChar3->addDescriptor(new BLE2902());

  uint8_t empty[20] = {0};
  bleChar1->setValue(empty, 20);
  bleChar2->setValue(empty, 20);
  bleChar3->setValue(empty, 20);

  service->start();

  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(BLEUUID((uint16_t)0x1FF7));
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);

  BLEDevice::startAdvertising();
  Serial.println("BLE advertising started");
}

void sendRaceChronoPackets() {
  if (!bleConnected) return;
  if (!tempEverValid) return;
  if (millis() - lastGoodFrameMs > TEMP_STALE_MS) return;
  if (millis() - bleConnectedAtMs < 800) return;

  updateBattery(false);

  uint8_t pkt1[20], pkt2[20], pkt3[20];
  buildPacket1(pkt1);
  buildPacket2(pkt2);
  buildPacket3(pkt3);

  bleChar1->setValue(pkt1, 20);
  bleChar1->notify();
  delay(1);

  bleChar2->setValue(pkt2, 20);
  bleChar2->notify();
  delay(1);

  bleChar3->setValue(pkt3, 20);
  bleChar3->notify();
}

// =====================================================
// MLX90640
// =====================================================
void configureMLXProfile() {
  Wire.setClock(mlxClock());
  mlx.setMode(MLX90640_CHESS);

  if (mlxFastProfile) {
    mlx.setResolution(MLX90640_ADC_17BIT);
    mlx.setRefreshRate(MLX90640_32_HZ);
    Serial.println("MLX profile = FAST_CLEAN, 1MHz I2C, 32Hz, 17bit ADC");
  } else {
    mlx.setResolution(MLX90640_ADC_18BIT);
    mlx.setRefreshRate(MLX90640_16_HZ);
    Serial.println("MLX profile = ROCK_STABLE, 400k I2C, 16Hz, 18bit ADC");
  }
}

void initMLX90640() {
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_STABLE_CLOCK);
  delay(100);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX90640 not found");
    while (true) delay(1000);
  }

  configureMLXProfile();
}

bool recoverMLX90640() {
  mlxRecoverCount++;
  Serial.print("MLX recover #");
  Serial.println(mlxRecoverCount);

  if (mlxFastProfile && mlxRecoverCount >= FAST_RECOVERIES_BEFORE_STABLE) {
    mlxFastProfile = false;
    Serial.println("Too many MLX bus errors; falling back to stable profile");
  }

  Wire.end();
  delay(30);
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_STABLE_CLOCK);
  delay(30);

  if (!mlx.begin(MLX90640_I2CADDR_DEFAULT, &Wire)) {
    Serial.println("MLX recover failed");
    return false;
  }

  configureMLXProfile();
  badFrameStreak = 0;
  return true;
}

int findBestROIRowStart(const float* rowScore, const int* rowValid) {
  int searchMin = clampInt(ROI_SEARCH_MIN_ROW, 0, FRAME_ROWS - ROI_HEIGHT);
  int searchMax = clampInt(ROI_SEARCH_MAX_ROW, 0, FRAME_ROWS - ROI_HEIGHT);
  int bestStart = roiRowStart;
  float bestScore = -1000000.0f;

  for (int start = searchMin; start <= searchMax; start++) {
    float sum = 0.0f;
    int rows = 0;

    for (int r = start; r < start + ROI_HEIGHT; r++) {
      if (rowValid[r] > 0) {
        sum += rowScore[r];
        rows++;
      }
    }

    if (rows < ROI_HEIGHT / 2) continue;

    float score = sum / rows;
    score -= abs(start - roiRowStart) * 0.16f;

    if (score > bestScore) {
      bestScore = score;
      bestStart = start;
    }
  }

  return bestStart;
}

void updateZonesFromFrame(uint32_t now) {
  float rowScore[FRAME_ROWS] = {0};
  int rowValid[FRAME_ROWS] = {0};

  for (int row = 0; row < FRAME_ROWS; row++) {
    float samples[FRAME_COLS];
    int count = 0;

    for (int col = 0; col < FRAME_COLS; col++) {
      float t = frameBuf[row * FRAME_COLS + col];
      if (!validTemp(t)) continue;
      samples[count++] = t;
    }

    rowValid[row] = count;
    rowScore[row] = trimmedMean(samples, count, TRIM_PIXELS_PER_ROW_SIDE);
  }

  int bestStart = findBestROIRowStart(rowScore, rowValid);
  bestStart = clampInt(bestStart,
                       roiRowStart - MAX_ROI_STEP_PER_FRAME,
                       roiRowStart + MAX_ROI_STEP_PER_FRAME);

  roiRowStartFiltered =
      roiRowStartFiltered * (1.0f - ROI_TRACK_ALPHA) +
      bestStart * ROI_TRACK_ALPHA;
  roiRowStart = clampInt((int)(roiRowStartFiltered + 0.5f), 0, FRAME_ROWS - ROI_HEIGHT);

  float zoneSamples[ZONE_COUNT][ZONE_PIXELS];
  int zoneCount[ZONE_COUNT] = {0};

  for (int row = roiRowStart; row < roiRowStart + ROI_HEIGHT; row++) {
    for (int col = 0; col < FRAME_COLS; col++) {
      float t = frameBuf[row * FRAME_COLS + col];
      if (!validTemp(t)) continue;

      int zone = col / ZONE_COLS;
      if (mirrorZones) zone = (ZONE_COUNT - 1) - zone;
      zone = clampInt(zone, 0, ZONE_COUNT - 1);

      if (zoneCount[zone] < ZONE_PIXELS) {
        zoneSamples[zone][zoneCount[zone]++] = t;
      }
    }
  }

  float rawZone[ZONE_COUNT] = {0};
  for (int i = 0; i < ZONE_COUNT; i++) {
    if (zoneCount[i] < MIN_VALID_PIXELS_PER_ZONE) {
      Serial.println("MLX zone rejected: too few valid pixels");
      return;
    }

    rawZone[i] = trimmedMean(zoneSamples[i], zoneCount[i], TRIM_PIXELS_PER_ZONE_SIDE);
    if (!validTemp(rawZone[i])) return;
    rawZone[i] = clampFloat(rawZone[i], TEMP_MIN_C, TEMP_MAX_C);
  }

  bool reinitFilter = !tempFilterInited ||
                      lastGoodFrameMs == 0 ||
                      now - lastGoodFrameMs > TEMP_STALE_MS;

  float dt = lastGoodFrameMs == 0 ? 0.04f : (now - lastGoodFrameMs) / 1000.0f;
  if (dt < 0.01f) dt = 0.01f;
  if (dt > 0.20f) dt = 0.20f;
  float maxStep = TEMP_MAX_RATE_C_PER_S * dt;

  for (int i = 0; i < ZONE_COUNT; i++) {
    if (reinitFilter) {
      zoneFiltered[i] = rawZone[i];
      zoneLastRaw[i] = rawZone[i];
    } else {
      float rawJump = fabs(rawZone[i] - zoneLastRaw[i]);
      float target = rawZone[i];

      if (rawJump > RAW_SPIKE_GATE_C && fabs(rawZone[i] - zoneFiltered[i]) > RAW_SPIKE_GATE_C) {
        target = limitStep(zoneFiltered[i], rawZone[i], maxStep);
      } else {
        target = limitStep(zoneFiltered[i], rawZone[i], maxStep);
      }

      float delta = fabs(target - zoneFiltered[i]);
      if (delta > TEMP_DEADBAND_C) {
        float alpha = tempAlphaForDelta(delta);
        zoneFiltered[i] += (target - zoneFiltered[i]) * alpha;
      }

      zoneLastRaw[i] = rawZone[i];
    }

    zoneOut10[i] = (int16_t)roundf(zoneFiltered[i] * 10.0f);
  }

  tempFilterInited = true;
  goodFrameCount++;
  lastGoodFrameMs = now;

  if (goodFrameCount > WARMUP_GOOD_FRAMES) {
    tempEverValid = true;
  }
}

void updateMLX90640() {
  Wire.setClock(mlxClock());
  int err = mlx.getFrame(frameBuf);

  if (err != 0) {
    badFrameStreak++;
    uint32_t now = millis();

    if (now - lastMlxErrLogMs >= MLX_ERR_LOG_INTERVAL_MS) {
      lastMlxErrLogMs = now;
      Serial.print("MLX getFrame err=");
      Serial.print(err);
      Serial.print(" badStreak=");
      Serial.println(badFrameStreak);
    }

    if (badFrameStreak >= MLX_RECOVER_BAD_STREAK) {
      recoverMLX90640();
    }
    return;
  }

  badFrameStreak = 0;
  updateZonesFromFrame(millis());
}

// =====================================================
// VL53L1X
// =====================================================
void initToF() {
  tofReady = false;
  Wire.setClock(I2C_STABLE_CLOCK);

  pinMode(VL53_XSHUT_PIN, OUTPUT);
  digitalWrite(VL53_XSHUT_PIN, LOW);
  delay(10);
  digitalWrite(VL53_XSHUT_PIN, HIGH);
  delay(60);

  tof.setTimeout(100);

  if (!tof.init()) {
    Serial.println("VL53L1X not found; height disabled");
    tofTravelMm = 0;
    return;
  }

  if (TOF_USE_SHORT_MODE) {
    tof.setDistanceMode(VL53L1X::Short);
  } else {
    tof.setDistanceMode(VL53L1X::Long);
  }

  tof.setMeasurementTimingBudget(20000);
  tof.startContinuous(20);

  tofDistanceRawMm = -1;
  tofDistanceFilteredMm = -1.0f;
  tofTravelMm = 0;
  tofBaselineReady = false;
  tofBaselineStartMs = millis();
  tofBaselineAccum = 0;
  tofBaselineCount = 0;
  tofReady = true;

  Serial.println("VL53L1X init OK");
}

void updateToF() {
  if (!tofReady) {
    tofTravelMm = 0;
    return;
  }

  Wire.setClock(I2C_STABLE_CLOCK);
  int dist = tof.read();

  if (tof.timeoutOccurred()) return;
  if (dist < TOF_VALID_MIN_MM || dist > TOF_VALID_MAX_MM) return;

  tofDistanceRawMm = dist;

  if (tofDistanceFilteredMm < 0.0f) {
    tofDistanceFilteredMm = (float)dist;
  } else {
    tofDistanceFilteredMm =
        tofDistanceFilteredMm * (1.0f - TOF_FILTER_ALPHA) +
        dist * TOF_FILTER_ALPHA;
  }

  if (!tofBaselineReady) {
    tofBaselineAccum += (long)(tofDistanceFilteredMm + 0.5f);
    tofBaselineCount++;

    if (millis() - tofBaselineStartMs >= TOF_BASELINE_TIME_MS) {
      if (tofBaselineCount > 0) {
        tofBaselineMm = (int)(tofBaselineAccum / tofBaselineCount);
      } else {
        tofBaselineMm = (int)(tofDistanceFilteredMm + 0.5f);
      }
      tofBaselineReady = true;
      Serial.print("Height zero auto = ");
      Serial.print(tofBaselineMm);
      Serial.println(" mm");
    }
  }

  if (tofBaselineReady) {
    tofTravelMm = tofBaselineMm - (int)(tofDistanceFilteredMm + 0.5f);
  } else {
    tofTravelMm = 0;
  }
}

// =====================================================
// Setup / loop
// =====================================================
void setup() {
  pinMode(PWR_KILL_PIN, OUTPUT);
  digitalWrite(PWR_KILL_PIN, HIGH);
  pinMode(PWR_INT_PIN, INPUT_PULLUP);

  pinMode(LED_STAT_PIN, OUTPUT);
  setStatusLed(false);

  pinMode(VL53_XSHUT_PIN, OUTPUT);
  digitalWrite(VL53_XSHUT_PIN, HIGH);
  pinMode(VL53_INT_PIN, INPUT_PULLUP);

  bootMs = millis();

  Serial.begin(115200);
  delay(120);

  analogReadResolution(12);
#ifdef ESP32
  analogSetPinAttenuation(VBAT_ADC_PIN, ADC_11db);
#endif

  setupButtonAndMirror();
  updateBattery(true);

  Serial.println();
  Serial.println("Boot RaceChrono fast clean V3 firmware");
  Serial.print("BLE name: ");
  Serial.println(DEVICE_NAME);
  Serial.print("Tire ID: ");
  Serial.println(TIRE_ID);
  Serial.print("Mirror: ");
  Serial.println(mirrorZones ? "ON" : "OFF");
  Serial.println("BLE only. No WiFi, no ESP-NOW, no central display link.");

  initMLX90640();
  initToF();
  setupBLE();

  startLedFlash(3, 80, 80);
}

void loop() {
  handleButton();
  updateStatusLed();

  uint32_t now = millis();

  if (now - lastTofMs >= TOF_INTERVAL_MS) {
    lastTofMs = now;
    updateToF();
  }

  if (now - lastFrameMs >= frameIntervalMs()) {
    lastFrameMs = now;
    updateMLX90640();
  }

  if (now - lastSendMs >= SEND_INTERVAL_MS) {
    lastSendMs = now;
    sendRaceChronoPackets();
  }

  if (now - lastDebugMs >= 1000) {
    lastDebugMs = now;
    Serial.print("BLE=");
    Serial.print(bleConnected);
    Serial.print(" profile=");
    Serial.print(mlxFastProfile ? "FAST" : "STABLE");
    Serial.print(" good=");
    Serial.print(goodFrameCount);
    Serial.print(" bad=");
    Serial.print(badFrameStreak);
    Serial.print(" ROI=");
    Serial.print(roiRowStart);
    Serial.print("-");
    Serial.print(roiRowStart + ROI_HEIGHT - 1);
    Serial.print(" travel=");
    Serial.print(tofTravelMm);
    Serial.print("mm batt=");
    Serial.print(batteryMv);
    Serial.print("mV temp=");
    for (int i = 0; i < ZONE_COUNT; i++) {
      Serial.print(zoneOut10[i] / 10.0f, 1);
      Serial.print(" ");
    }
    Serial.println();
  }
}
