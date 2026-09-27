#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

/*
  MOCHI
  - BLE phone -> ESP32
  - OLED face with richer non-blocking animations
  - External digital touch sensor support (TTP223 or similar)
  - Tap / double tap / long press reactions
  - Pace-note style navigation HUD
  - Navigation can make Mochi "look" toward the maneuver
  - Non-blocking buzzer engine
  - Backward-compatible BLE packet:
      SPEED|MODE|DETAIL|CLOCK

  MAPS DETAIL:
      DIRECTION,DISTANCE,STREET,ETA,TOTALDIST
  Optional next maneuver:
      DIRECTION,DISTANCE,STREET,ETA,TOTALDIST,NEXTDIR,NEXTDIST

  Example:
      45|MAPS|RIGHT,150m,Jl Pemuda,10m,12.4km,LEFT,500m|16:42
*/

// ============================================================
// HARDWARE CONFIGURATION
// ============================================================

// Keep the original pins until your actual wiring is confirmed.
// IMPORTANT: ESP32-C3 boards normally do NOT have GPIO23.
// If using ESP32-C3, change this to the GPIO actually wired to the buzzer.
#define BUZZER_PIN 23

// External digital touch module, e.g. TTP223.
// Change to the GPIO actually connected to OUT.
#define TOUCH_PIN 4

// Set true if your touch module outputs HIGH when touched.
// TTP223 modules commonly do this.
#define TOUCH_ACTIVE_HIGH true

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDR 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ============================================================
// BLE
// ============================================================

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLECharacteristic *pCharacteristic = nullptr;
bool deviceConnected = false;

// ============================================================
// SYSTEM STATE
// ============================================================

String currentSpeed = "0";
String currentMode = "IDLE";
String currentDetail = "NORMAL";
String currentClock = "08:45";

// ============================================================
// NAVIGATION STATE
// ============================================================

String navDirection = "STRAIGHT";
String navDistance = "150m";
String navStreet = "Jl. Pemuda";
String navETA = "10m";
String navTotalDist = "12.4km";

String nextDirection = "STRAIGHT";
String nextDistance = "";

// ============================================================
// FACE / BEHAVIOR STATE
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

  // Repeating warning/panic beep without delay()
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
// SMALL HELPERS
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

// ============================================================
// MAPS PARSER
// ============================================================

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

  for (int i = 0; i < index; i++) {
    parts[i].trim();
  }

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

  if (index >= 7) {
    nextDistance = parts[6];
  } else {
    nextDistance = "";
  }
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
    else if (currentMode == "RIDING") {
      setFace(FACE_FOCUSED);
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

  // During navigation, touch means "I'm here / acknowledge"
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

  // Resolve single/double tap after the waiting window
  if (!touchStable && tapCount > 0 && timePassed(now, lastTapTime, DOUBLE_TAP_WINDOW)) {
    if (tapCount >= 2) onDoubleTap();
    else onSingleTap();

    tapCount = 0;
  }
}

// ============================================================
// EYE / FACE ANIMATION ENGINE
// ============================================================

void updateFaceAnimation() {
  unsigned long now = millis();

  // Return temporary expression to normal/focused state
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

  // Smooth eye movement
  if (eyeLookX < targetLookX) eyeLookX++;
  if (eyeLookX > targetLookX) eyeLookX--;
  if (eyeLookY < targetLookY) eyeLookY++;
  if (eyeLookY > targetLookY) eyeLookY--;

  // Blink
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

  // Idle micro-behavior
  if (timePassed(now, lastIdleAction, nextIdleAction)) {
    lastIdleAction = now;
    nextIdleAction = random(2500, 7000);

    if (currentMode == "MAPS") {
      // Look toward navigation
      if (navDirection.indexOf("LEFT") >= 0) lookAt(-4, 0);
      else if (navDirection.indexOf("RIGHT") >= 0) lookAt(4, 0);
      else lookAt(0, -1);
    }
    else if (currentMode == "RIDING") {
      // Focus forward, occasional side glance
      if (random(0, 5) == 0) randomIdleLook();
      else lookAt(0, 0);
    }
    else if (face == FACE_NORMAL) {
      randomIdleLook();

      // Small spontaneous expressions
      int r = random(0, 100);

      if (r < 7) {
        setFace(FACE_CURIOUS, 1300);
      } else if (r < 11) {
        setFace(FACE_CONFUSED, 1500);
      } else if (r < 14) {
        setFace(FACE_SLEEPY, 1800);
      } else if (r < 17) {
        setFace(FACE_SURPRISED, 800);
      }
    }
  }

  // Small mouth animation for excited/happy
  if (timePassed(now, lastTouchEvent, 1000)) {
    mouthOpen = false;
  }
}

// ============================================================
// FACE DRAWING
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

  if (mouthOpen) {
    display.fillRoundRect(60, 45, 8, 9, 3, WHITE);
  } else {
    display.fillCircle(64, 49, 3, WHITE);
  }
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

void drawSadFace() {
  display.fillRoundRect(25, 29, 20, 17, 6, WHITE);
  display.fillRoundRect(83, 29, 20, 17, 6, WHITE);

  display.drawLine(56, 51, 62, 47, WHITE);
  display.drawLine(62, 47, 68, 47, WHITE);
  display.drawLine(68, 47, 74, 51, WHITE);
}

void drawAngryFace() {
  display.drawLine(19, 19, 48, 28, WHITE);
  display.drawLine(109, 19, 80, 28, WHITE);

  display.fillRoundRect(23, 30, 23, 17, 5, WHITE);
  display.fillRoundRect(82, 30, 23, 17, 5, WHITE);

  display.fillRoundRect(59, 45, 10, 11, 3, WHITE);
  display.fillRect(62, 47, 4, 6, BLACK);
}

void drawSurprisedFace() {
  display.fillCircle(35, 34, 14, WHITE);
  display.fillCircle(93, 34, 14, WHITE);

  display.fillCircle(35 + eyeLookX, 34 + eyeLookY, 4, BLACK);
  display.fillCircle(93 + eyeLookX, 34 + eyeLookY, 4, BLACK);

  display.drawCircle(64, 49, 6, WHITE);
  display.fillCircle(64, 49, 2, BLACK);
}

void drawConfusedFace() {
  display.fillRoundRect(24, 27, 22, 20, 6, WHITE);
  display.fillCircle(93, 35, 9, WHITE);

  display.drawLine(23, 21, 46, 18, WHITE);
  display.drawLine(82, 24, 104, 27, WHITE);

  display.drawLine(57, 49, 71, 46, WHITE);
}

void drawSleepyFace() {
  display.drawLine(23, 36, 46, 39, WHITE);
  display.drawLine(82, 39, 105, 36, WHITE);

  display.fillRoundRect(60, 47, 8, 6, 3, WHITE);
}

void drawSleepFace() {
  display.fillRect(24, 37, 22, 3, WHITE);
  display.fillRect(82, 37, 22, 3, WHITE);

  display.setTextSize(1);
  display.setCursor(106, 19);
  display.print("z");
  display.setCursor(115, 11);
  display.print("Z");
}

void drawExcitedFace() {
  display.fillCircle(35, 34, 13, WHITE);
  display.fillCircle(93, 34, 13, WHITE);

  display.fillCircle(35 + eyeLookX, 34 + eyeLookY, 4, BLACK);
  display.fillCircle(93 + eyeLookX, 34 + eyeLookY, 4, BLACK);

  display.fillCircle(64, 49, 6, WHITE);
  display.fillRect(60, 45, 8, 4, BLACK);
}

void drawScaredFace() {
  display.fillCircle(35, 34, 11, WHITE);
  display.fillCircle(93, 34, 11, WHITE);

  display.fillCircle(35, 34, 6, BLACK);
  display.fillCircle(93, 34, 6, BLACK);

  display.fillRoundRect(59, 46, 10, 10, 5, WHITE);
}

void drawFocusedFace() {
  display.fillRoundRect(24, 28, 23, 18, 6, WHITE);
  display.fillRoundRect(81, 28, 23, 18, 6, WHITE);

  display.fillRect(30 + eyeLookX, 34 + eyeLookY, 7, 6, BLACK);
  display.fillRect(87 + eyeLookX, 34 + eyeLookY, 7, 6, BLACK);

  display.drawLine(58, 49, 70, 49, WHITE);
}

void drawRelievedFace() {
  display.drawLine(24, 37, 34, 33, WHITE);
  display.drawLine(34, 33, 44, 37, WHITE);

  display.drawLine(84, 37, 94, 33, WHITE);
  display.drawLine(94, 33, 104, 37, WHITE);

  display.drawLine(58, 49, 64, 52, WHITE);
  display.drawLine(64, 52, 70, 49, WHITE);
}

void drawCuriousFace() {
  display.fillRoundRect(24, 27, 23, 22, 7, WHITE);
  display.fillRoundRect(82, 24, 23, 25, 7, WHITE);

  display.fillCircle(35 + eyeLookX, 36 + eyeLookY, 5, BLACK);
  display.fillCircle(93 + eyeLookX, 35 + eyeLookY, 5, BLACK);

  display.drawLine(58, 48, 66, 45, WHITE);
  display.drawLine(66, 45, 71, 48, WHITE);
}

void drawEmbarrassedFace() {
  drawEye(25, 30, 20, 17);
  drawEye(83, 30, 20, 17);

  display.drawLine(58, 49, 70, 49, WHITE);

  // Tiny blush pixels
  display.fillRect(18, 48, 8, 2, WHITE);
  display.fillRect(102, 48, 8, 2, WHITE);
}

void renderFace() {
  switch (face) {
    case FACE_HAPPY:      drawHappyFace(); break;
    case FACE_SAD:        drawSadFace(); break;
    case FACE_ANGRY:      drawAngryFace(); break;
    case FACE_SURPRISED:  drawSurprisedFace(); break;
    case FACE_CONFUSED:   drawConfusedFace(); break;
    case FACE_SLEEPY:     drawSleepyFace(); break;
    case FACE_SLEEP:      drawSleepFace(); break;
    case FACE_EXCITED:    drawExcitedFace(); break;
    case FACE_SCARED:     drawScaredFace(); break;
    case FACE_FOCUSED:    drawFocusedFace(); break;
    case FACE_RELIEVED:   drawRelievedFace(); break;
    case FACE_CURIOUS:    drawCuriousFace(); break;
    case FACE_EMBARRASSED:drawEmbarrassedFace(); break;
    default:              drawNormalFace(); break;
  }
}

// ============================================================
// NAVIGATION HUD
// ============================================================

void drawArrowLarge(String dir, int cx, int cy) {
  dir.toUpperCase();

  if (dir == "LEFT" || dir == "SLIGHT_LEFT") {
    display.fillTriangle(cx - 20, cy, cx + 2, cy - 18, cx + 2, cy + 18, WHITE);
    display.fillRect(cx + 2, cy - 5, 22, 10, WHITE);

    if (dir == "SLIGHT_LEFT") {
      display.drawLine(cx + 8, cy + 10, cx + 22, cy + 22, BLACK);
    }
  }
  else if (dir == "RIGHT" || dir == "SLIGHT_RIGHT") {
    display.fillTriangle(cx + 20, cy, cx - 2, cy - 18, cx - 2, cy + 18, WHITE);
    display.fillRect(cx - 24, cy - 5, 22, 10, WHITE);

    if (dir == "SLIGHT_RIGHT") {
      display.drawLine(cx - 8, cy + 10, cx - 22, cy + 22, BLACK);
    }
  }
  else if (dir == "UTURN") {
    display.drawCircle(cx, cy + 2, 17, WHITE);
    display.fillRect(cx - 20, cy + 2, 8, 19, BLACK);
    display.fillRect(cx + 12, cy + 2, 8, 12, BLACK);
    display.fillTriangle(cx - 24, cy + 20, cx - 8, cy + 20, cx - 16, cy + 29, WHITE);
  }
  else {
    display.fillTriangle(cx, cy - 22, cx - 14, cy - 4, cx + 14, cy - 4, WHITE);
    display.fillRect(cx - 5, cy - 4, 10, 25, WHITE);
  }
}

void renderPaceNoteHUD() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  // Header
  display.setCursor(0, 0);
  display.print(currentClock);

  display.setCursor(45, 0);
  display.print(currentSpeed);
  display.print("km/h");

  display.setCursor(100, 0);
  display.print(deviceConnected ? "B" : "-");

  display.drawFastHLine(0, 9, 128, WHITE);

  // Main maneuver
  drawArrowLarge(navDirection, 30, 35);

  display.setCursor(8, 54);
  display.print(shortText(navDistance, 9));

  // Right information block
  display.setCursor(59, 14);
  display.setTextSize(1);
  display.print(shortText(navStreet, 11));

  display.setTextSize(2);
  display.setCursor(58, 25);
  display.print(shortText(navDistance, 7));

  display.setTextSize(1);
  display.setCursor(58, 43);

  if (navDirection == "LEFT" || navDirection == "SLIGHT_LEFT") {
    display.print("< LEFT");
  } else if (navDirection == "RIGHT" || navDirection == "SLIGHT_RIGHT") {
    display.print("RIGHT >");
  } else if (navDirection == "UTURN") {
    display.print("U-TURN");
  } else {
    display.print("STRAIGHT");
  }

  display.setCursor(58, 54);
  display.print("N:");
  display.print(shortText(nextDistance, 5));

  // Pace-note vertical separator
  display.drawFastVLine(52, 10, 54, WHITE);

  // Near-turn warning: invert a small area when very close
  String d = navDistance;
  d.toLowerCase();

  bool closeTurn =
    d.indexOf("20m") >= 0 ||
    d.indexOf("10m") >= 0 ||
    d.indexOf("5m") >= 0;

  if (closeTurn) {
    display.drawRect(54, 10, 73, 53, WHITE);
  }
}

// ============================================================
// FACE MODE HEADER
// ============================================================

void renderFaceHeader() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print(currentClock);

  display.setCursor(100, 0);
  display.print(deviceConnected ? "BLE" : "---");

  // Riding speed
  if (currentMode == "RIDING") {
    display.setCursor(42, 55);
    display.print(currentSpeed);
    display.print(" KM/H");
  }
}

// ============================================================
// SETUP
// ============================================================

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  pinMode(TOUCH_PIN, TOUCH_ACTIVE_HIGH ? INPUT : INPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    while (true) {
      delay(100);
    }
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(16, 22);
  display.print("MOCHI");
  display.setCursor(16, 36);
  display.print("Starting...");
  display.display();
  delay(700);

  randomSeed(micros());

  BLEDevice::init("Mochi-ESP32");

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);

  pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->addDescriptor(new BLE2902());
  pCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);

  BLEDevice::startAdvertising();

  lastIdleAction = millis();
  blinkStart = millis();
  touchChangeTime = millis();

  setFace(FACE_SLEEP);
}

// ============================================================
// LOOP
// ============================================================

void loop() {
  updateTouch();
  updateFaceAnimation();
  updateBuzzer();

  display.clearDisplay();

  if (currentMode == "MAPS") {
    renderPaceNoteHUD();
  } else {
    renderFace();
    renderFaceHeader();
  }

  display.display();

  // Short yield instead of long blocking delays
  delay(10);
}
