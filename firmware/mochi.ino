#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/*
  MOCHI - Smart Vehicle Companion
  - BLE Phone -> ESP32
  - OLED Face & Non-blocking Pace-Note HUD
  - Touch Sensor Support (TTP223)
  - Non-blocking Buzzer Engine
  - Payload Format: SPEED|MODE|DETAIL|CLOCK
*/

// ============================================================
// HARDWARE CONFIGURATION
// ============================================================
// Sesuaikan BUZZER_PIN dengan board Anda (misal GPIO 23 untuk ESP32 Wroom, atau GPIO 5/2 untuk ESP32-C3)
#define BUZZER_PIN 23
#define TOUCH_PIN 4
#define TOUCH_ACTIVE_HIGH true

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================
// BLE CONFIGURATION
// ============================================================
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic = nullptr;
bool deviceConnected = false;

// ============================================================
// SYSTEM & NAVIGATION STATE
// ============================================================
String currentSpeed = "0";
String currentMode = "IDLE";
String currentDetail = "NORMAL";
String currentClock = "08:45";

String navDirection = "STRAIGHT";
String navDistance = "150m";
String navStreet = "Jl. Pemuda";
String navETA = "10m";
String navTotalDist = "12.4km";
String nextDirection = "STRAIGHT";
String nextDistance = "";

// ============================================================
// FACE EXPRESSION STATE
// ============================================================
enum FaceExpression {
  FACE_NORMAL,
  FACE_HAPPY,
  FACE_SAD,
  FACE_ANGRY,
  FACE_SURPRISED,
  FACE_CONFUSED,
  FACE_SLEEPY,
  FACE_SLEEP,
  FACE_EXCITED,
  FACE_SCARED,
  FACE_FOCUSED,
  FACE_RELIEVED,
  FACE_CURIOUS,
  FACE_EMBARRASSED
};

FaceExpression face = FACE_NORMAL;
FaceExpression targetFace = FACE_NORMAL;

unsigned long faceUntil = 0;
unsigned long lastIdleAction = 0;
unsigned long nextIdleAction = 5000;

int eyeLookX = 0;
int eyeLookY = 0;
int targetLookX = 0;
int targetLookY = 0;

bool blink = false;
unsigned long blinkStart = 0;
unsigned long blinkDuration = 130;
bool mouthOpen = false;

// ============================================================
// TOUCH STATE
// ============================================================
bool touchStable = false;
bool lastTouchReading = false;
unsigned long touchChangeTime = 0;
unsigned long touchStartTime = 0;
unsigned long lastTapTime = 0;
unsigned long lastTouchEvent = 0;
uint8_t tapCount = 0;

const unsigned long TOUCH_DEBOUNCE = 35;
const unsigned long DOUBLE_TAP_WINDOW = 350;
const unsigned long LONG_PRESS_TIME = 650;

// ============================================================
// BUZZER ENGINE
// ============================================================
bool buzzerOn = false;
unsigned long buzzerUntil = 0;
unsigned long nextWarningBeep = 0;
uint8_t warningPhase = 0;

void buzzerSet(bool on) {
  buzzerOn = on;
  digitalWrite(BUZZER_PIN, on ? HIGH : LOW);
}

void beep(uint16_t durationMs) {
  buzzerSet(true);
  buzzerUntil = millis() + durationMs;
}

void updateBuzzer() {
  unsigned long now = millis();

  if (buzzerOn && (long)(now - buzzerUntil) >= 0) {
    buzzerSet(false);
  }

  bool panic = (currentDetail == "RIDE_PANIC" || currentMode == "PANIC");

  if (panic) {
    if ((long)(now - nextWarningBeep) >= 0) {
      warningPhase++;
      if (warningPhase % 2 == 1) {
        beep(70);
        nextWarningBeep = now + 180;
      } else {
        nextWarningBeep = now + 280;
      }
    }
  } else {
    warningPhase = 0;
  }
}

// ============================================================
// HELPERS & PARSER
// ============================================================
bool timePassed(unsigned long now, unsigned long timestamp, unsigned long interval) {
  return (unsigned long)(now - timestamp) >= interval;
}

String shortText(String s, uint8_t maxLen) {
  s.trim();
  if (s.length() <= maxLen) return s;
  if (maxLen < 2) return s.substring(0, maxLen);
  return s.substring(0, maxLen - 1) + ".";
}

void setFace(FaceExpression newFace, unsigned long duration = 0) {
  targetFace = newFace;
  face = newFace;
  faceUntil = duration ? millis() + duration : 0;
}

void lookAt(int x, int y) {
  targetLookX = constrain(x, -5, 5);
  targetLookY = constrain(y, -3, 3);
}

void randomIdleLook() {
  targetLookX = random(-5, 6);
  targetLookY = random(-2, 3);
}

void parseMapsData(String data) {
  data.trim();

  String parts[7];
  int start = 0;
  int index = 0;

  while (index < 7) {
    int comma = data.indexOf(',', start);
    if (comma < 0) {
      parts[index++] = data.substring(start);
      break;
    }
    parts[index++] = data.substring(start, comma);
    start = comma + 1;
  }

  for (int i = 0; i < index; i++) parts[i].trim();

  if (index >= 1 && parts[0].length()) {
    navDirection = parts[0];
    navDirection.toUpperCase();
  }
  if (index >= 2 && parts[1].length()) navDistance = parts[1];
  if (index >= 3 && parts[2].length()) navStreet = parts[2];
  if (index >= 4 && parts[3].length()) navETA = parts[3];
  if (index >= 5) navTotalDist = parts[4];

  if (index >= 6 && parts[5].length()) {
    nextDirection = parts[5];
    nextDirection.toUpperCase();
  } else {
    nextDirection = "STRAIGHT";
  }

  if (index >= 7) nextDistance = parts[6];
  else nextDistance = "";
}

// ============================================================
// BLE CALLBACKS
// ============================================================
class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    setFace(FACE_HAPPY, 1800);
    beep(80);
  }

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    setFace(FACE_SLEEP);
    buzzerSet(false);
    BLEDevice::startAdvertising();
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue().c_str();
    rxValue.trim();

    if (!rxValue.length()) return;

    int p1 = rxValue.indexOf('|');
    int p2 = rxValue.indexOf('|', p1 + 1);
    int p3 = rxValue.indexOf('|', p2 + 1);

    if (p1 < 0 || p2 < 0 || p3 < 0) return;

    currentSpeed = rxValue.substring(0, p1);
    currentMode = rxValue.substring(p1 + 1, p2);
    currentDetail = rxValue.substring(p2 + 1, p3);
    currentClock = rxValue.substring(p3 + 1);

    currentMode.trim();
    currentMode.toUpperCase();
    currentDetail.trim();

    if (currentMode == "MAPS") {
      parseMapsData(currentDetail);
      setFace(FACE_FOCUSED);
    }
    else if (currentMode == "ANGRY" || currentDetail == "RIDE_PANIC" || currentMode == "PANIC") {
      setFace(FACE_ANGRY);
    }
    else if (currentMode == "HAPPY") {
      setFace(FACE_HAPPY);
    }
    else if (currentMode == "SURPRISED") {
      setFace(FACE_SURPRISED, 1600);
    }
    else if (currentMode == "CONFUSED") {
      setFace(FACE_CONFUSED, 2000);
    }
    else if (currentMode == "SLEEP") {
      setFace(FACE_SLEEP);
    }
    else if (currentMode == "RIDING" || currentMode == "NORMAL") {
      setFace(FACE_NORMAL);
    }
  }
};

// ============================================================
// TOUCH ENGINE
// ============================================================
bool readTouchRaw() {
  int value = digitalRead(TOUCH_PIN);
  return TOUCH_ACTIVE_HIGH ? (value == HIGH) : (value == LOW);
}

void onSingleTap() {
  lastTouchEvent = millis();
  if (currentMode == "MAPS") {
    setFace(FACE_HAPPY, 1000);
    beep(45);
    return;
  }
  setFace(FACE_HAPPY, 1100);
  randomIdleLook();
  mouthOpen = true;
  beep(45);
}

void onDoubleTap() {
  lastTouchEvent = millis();
  setFace(FACE_EXCITED, 1500);
  lookAt(0, -2);
  beep(70);
}

void onLongPress() {
  lastTouchEvent = millis();
  setFace(FACE_SLEEPY, 1700);
  lookAt(0, 3);
  beep(100);
}

void updateTouch() {
  unsigned long now = millis();
  bool reading = readTouchRaw();

  if (reading != lastTouchReading) {
    touchChangeTime = now;
    lastTouchReading = reading;
  }

  if (!timePassed(now, touchChangeTime, TOUCH_DEBOUNCE)) return;

  if (reading != touchStable) {
    touchStable = reading;
    if (touchStable) {
      touchStartTime = now;
    } else {
      unsigned long held = now - touchStartTime;
      if (held >= LONG_PRESS_TIME) {
        tapCount = 0;
        onLongPress();
      } else {
        if (timePassed(now, lastTapTime, DOUBLE_TAP_WINDOW)) {
          tapCount = 1;
        } else {
          tapCount++;
        }
        lastTapTime = now;
      }
    }
  }

  if (!touchStable && tapCount > 0 && timePassed(now, lastTapTime, DOUBLE_TAP_WINDOW)) {
    if (tapCount >= 2) onDoubleTap();
    else onSingleTap();
    tapCount = 0;
  }
}

// ============================================================
// FACE ANIMATION ENGINE
// ============================================================
void updateFaceAnimation() {
  unsigned long now = millis();

  if (faceUntil && (long)(now - faceUntil) >= 0) {
    faceUntil = 0;
    if (currentMode == "RIDING" || currentMode == "MAPS") {
      setFace(FACE_FOCUSED);
    } else if (!deviceConnected) {
      setFace(FACE_SLEEP);
    } else {
      setFace(FACE_NORMAL);
    }
  }

  if (eyeLookX < targetLookX) eyeLookX++;
  if (eyeLookX > targetLookX) eyeLookX--;
  if (eyeLookY < targetLookY) eyeLookY++;
  if (eyeLookY > targetLookY) eyeLookY--;

  if (!blink && timePassed(now, blinkStart, random(2800, 5200))) {
    blink = true;
    blinkStart = now;
    blinkDuration = random(90, 150);
  }

  if (blink && timePassed(now, blinkStart, blinkDuration)) {
    blink = false;
    blinkStart = now;
    randomIdleLook();
  }

  if (timePassed(now, lastIdleAction, nextIdleAction)) {
    lastIdleAction = now;
    nextIdleAction = random(2500, 7000);

    if (currentMode == "MAPS") {
      if (navDirection.indexOf("LEFT") >= 0) lookAt(-4, 0);
      else if (navDirection.indexOf("RIGHT") >= 0) lookAt(4, 0);
      else lookAt(0, -1);
    } else if (face == FACE_NORMAL) {
      randomIdleLook();
      int r = random(0, 100);
      if (r < 7) setFace(FACE_CURIOUS, 1300);
      else if (r < 11) setFace(FACE_CONFUSED, 1500);
      else if (r < 14) setFace(FACE_SLEEPY, 1800);
      else if (r < 17) setFace(FACE_SURPRISED, 800);
    }
  }

  if (timePassed(now, lastTouchEvent, 1000)) {
    mouthOpen = false;
  }
}

// ============================================================
// DRAWING FUNCTIONS
// ============================================================
void drawEye(int x, int y, int w, int h, bool pupil = true) {
  display.fillRoundRect(x, y, w, h, min(8, w / 2), WHITE);
  if (pupil) {
    int px = x + 4 + eyeLookX;
    int py = y + 5 + eyeLookY;
    px = constrain(px, x + 2, x + w - 8);
    py = constrain(py, y + 2, y + h - 8);
    display.fillRoundRect(px, py, 6, 7, 2, BLACK);
    display.fillRect(px + 1, py + 1, 2, 2, WHITE);
  }
}

void drawNormalFace() {
  if (blink) {
    display.fillRoundRect(24, 35, 23, 4, 2, WHITE);
    display.fillRoundRect(81, 35, 23, 4, 2, WHITE);
  } else {
    drawEye(24 + eyeLookX, 25 + eyeLookY, 23, 24);
    drawEye(81 + eyeLookX, 25 + eyeLookY, 23, 24);
  }
  if (mouthOpen) display.fillRoundRect(60, 45, 8, 9, 3, WHITE);
  else display.fillCircle(64, 49, 3, WHITE);
}

void drawHappyFace() {
  display.drawLine(24, 38, 35, 32, WHITE);
  display.drawLine(35, 32, 46, 38, WHITE);
  display.drawLine(82, 38, 93, 32, WHITE);
  display.drawLine(93, 32, 104, 38, WHITE);
  display.drawLine(55, 46, 61, 50, WHITE);
  display.drawLine(61, 50, 67, 50, WHITE);
  display.drawLine(67, 50, 73, 46, WHITE);
}

void drawAngryFace() {
  display.drawLine(19, 19, 48, 28, WHITE);
  display.drawLine(109, 19, 80, 28, WHITE);
  display.fillRoundRect(23, 30, 23, 17, 5, WHITE);
  display.fillRoundRect(82, 30, 23, 17, 5, WHITE);
  display.fillRoundRect(59, 45, 10, 11, 3, WHITE);
  display.fillRect(62, 47, 4, 6, BLACK);
}

void drawSleepFace() {
  display.fillRect(24, 37, 22, 3, WHITE);
  display.fillRect(82, 37, 22, 3, WHITE);
  display.setTextSize(1);
  display.setCursor(106, 19); display.print("z");
  display.setCursor(115, 11); display.print("Z");
}

void renderFace() {
  switch (face) {
    case FACE_HAPPY:     drawHappyFace(); break;
    case FACE_ANGRY:     drawAngryFace(); break;
    case FACE_SLEEP:     drawSleepFace(); break;
    default:             drawNormalFace(); break;
  }
}

void drawArrowLarge(String dir, int cx, int cy) {
  dir.toUpperCase();
  if (dir == "LEFT" || dir == "SLIGHT_LEFT") {
    display.fillTriangle(cx - 20, cy, cx + 2, cy - 18, cx + 2, cy + 18, WHITE);
    display.fillRect(cx + 2, cy - 5, 22, 10, WHITE);
  } else if (dir == "RIGHT" || dir == "SLIGHT_RIGHT") {
    display.fillTriangle(cx + 20, cy, cx - 2, cy - 18, cx - 2, cy + 18, WHITE);
    display.fillRect(cx - 24, cy - 5, 22, 10, WHITE);
  } else {
    display.fillTriangle(cx, cy - 22, cx - 14, cy - 4, cx + 14, cy - 4, WHITE);
    display.fillRect(cx - 5, cy - 4, 10, 25, WHITE);
  }
}

// BAGIAN PACE NOTE HUD YANG DILENGKAPI
void renderPaceNoteHUD() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Header Bar
  display.setCursor(0, 0); display.print(currentClock);
  display.setCursor(45, 0); display.print(currentSpeed); display.print("km/h");
  display.setCursor(108, 0); display.print(deviceConnected ? "BLE" : "--");
  display.drawFastHLine(0, 9, 128, WHITE);

  // Main Nav Arrow & Distance
  drawArrowLarge(navDirection, 25, 33);
  display.setCursor(2, 54); display.print(shortText(navDistance, 7));

  // Street Info
  display.setCursor(55, 14); display.print(shortText(navStreet, 11));
  display.setTextSize(2);
  display.setCursor(55, 25); display.print(shortText(navDistance, 6));

  // Sub Info / Next Maneuver
  display.setTextSize(1);
  display.setCursor(55, 43); display.print(shortText(navDirection, 11));
  display.setCursor(55, 54);
  display.print("Nxt:"); display.print(shortText(nextDirection, 6));
}

// ============================================================
// ARDUINO SETUP & LOOP
// ============================================================
void setup() {
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(TOUCH_PIN, INPUT);
  buzzerSet(false);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    for (;;); // Stop jika OLED gagal terdeteksi
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(25, 25);
  display.print("MOCHI START...");
  display.display();
  delay(1000);

  // Inisialisasi BLE
  BLEDevice::init("MOCHI_HUD");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();

  beep(100);
}

void loop() {
  updateTouch();
  updateBuzzer();
  updateFaceAnimation();

  display.clearDisplay();
  if (currentMode == "MAPS") {
    renderPaceNoteHUD();
  } else {
    renderFace();
  }
  display.display();
}
