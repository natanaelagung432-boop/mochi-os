#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// --- KONFIGURASI PIN HARDWARE ---
#define TOUCH_PIN 4         // Pin Sensor Sentuh (GPIO 4)
#define TOUCH_THRESHOLD 40  // Sensitivitas Sentuhan
#define BUZZER_PIN 18       // Pin Buzzer Mochi

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
unsigned long lastDataTime = 0;

// Variabel Data dari HP
int currentSpeed = 0;
int speedLimit = 80;
String currentMood = "IDLE";
String currentTimeStr = "00:00";

// Mode Layar (0: Riding, 1: Clock)
int currentDisplayMode = 0; 

// Deteksi Multi-Tap & Alarm Timer
int tapCount = 0;
unsigned long lastTapTime = 0;
bool lastTouchState = false;
unsigned long lastAlarmTime = 0;

// --- FUNGSI SUARA BUZZER ---

void playBeepClick() {
  tone(BUZZER_PIN, 1000, 50);
}

void playHappySound() {
  tone(BUZZER_PIN, 523, 80); delay(90);
  tone(BUZZER_PIN, 659, 80); delay(90);
  tone(BUZZER_PIN, 784, 120); delay(130);
  noTone(BUZZER_PIN);
}

void playWarningAlarm() {
  tone(BUZZER_PIN, 2000, 100); delay(120);
  tone(BUZZER_PIN, 2000, 100); delay(120);
  noTone(BUZZER_PIN);
}

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { 
      deviceConnected = true; 
      playHappySound(); 
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();
      if (rxValue.length() > 0) {
        lastDataTime = millis();
        int p1 = rxValue.indexOf('|');
        int p2 = rxValue.indexOf('|', p1 + 1);
        int p3 = rxValue.indexOf('|', p2 + 1);

        if (p1 != -1) currentSpeed = rxValue.substring(0, p1).toInt();
        if (p2 != -1) currentMood = rxValue.substring(p1 + 1, p2);
        if (p3 != -1) currentTimeStr = rxValue.substring(p2 + 1, p3);
        if (p3 != -1) speedLimit = rxValue.substring(p3 + 1).toInt();
      }
    }
};

// --- FUNGSI TAMPILAN WAJAH & MULTI-MODE ---

void drawOverspeedShockFace() {
  display.clearDisplay();
  // Mata Kaget Belalak
  display.drawCircle(32, 28, 14, SSD1306_WHITE);
  display.drawCircle(96, 28, 14, SSD1306_WHITE);
  display.fillCircle(32, 28, 5, SSD1306_WHITE);
  display.fillCircle(96, 28, 5, SSD1306_WHITE);

  // Mulut "O" Kaget
  display.drawCircle(64, 48, 8, SSD1306_WHITE);

  // Peringatan Kecepatan Teks
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(20, 2);
  display.print("! OVER SPEED !");
  display.display();
}

void drawHappyFace() {
  display.clearDisplay();
  display.drawLine(25, 32, 35, 20, SSD1306_WHITE);
  display.drawLine(35, 20, 45, 32, SSD1306_WHITE);
  display.drawLine(83, 32, 93, 20, SSD1306_WHITE);
  display.drawLine(93, 20, 103, 32, SSD1306_WHITE);
  display.drawTriangle(58, 40, 70, 40, 64, 48, SSD1306_WHITE);
  display.display();
}

void drawRidingMode() {
  display.clearDisplay();
  display.fillRect(30, 24, 16, 16, SSD1306_WHITE);
  display.fillRect(82, 24, 16, 16, SSD1306_WHITE);

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(50, 20);
  display.print(currentSpeed);
  display.setTextSize(1);
  display.setCursor(52, 40);
  display.print("KM/H");
  display.display();
}

void drawClockMode() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(28, 10);
  display.print("JAM SEKARANG");

  display.setTextSize(3);
  display.setCursor(20, 28);
  display.print(currentTimeStr);
  display.display();
}

void checkTouchLogic() {
  bool isTouching = (touchRead(TOUCH_PIN) < TOUCH_THRESHOLD);

  if (isTouching && !lastTouchState) {
    unsigned long now = millis();
    if (now - lastTapTime < 500) {
      tapCount++;
    } else {
      tapCount = 1;
    }
    lastTapTime = now;
  }
  lastTouchState = isTouching;

  if (tapCount >= 3) {
    tapCount = 0;
    currentDisplayMode = (currentDisplayMode + 1) % 2; 
    playBeepClick();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for (;;);
  }
  display.clearDisplay();

  BLEDevice::init("DASAI_MOCHI");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ |
                      BLECharacteristic::PROPERTY_WRITE |
                      BLECharacteristic::PROPERTY_NOTIFY
                    );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();

  playBeepClick();
  drawRidingMode();
}

void loop() {
  checkTouchLogic();

  bool isHolding = (touchRead(TOUCH_PIN) < TOUCH_THRESHOLD) && (tapCount < 2);

  // PRIORITAS TAMPILAN & AKSI
  if (currentMood == "OVERSPEED") {
    // 1. Jika Kecepatan Melebihi Batas Limit
    drawOverspeedShockFace();

    // Bunyikan Alarm Setiap 1 Detik
    if (millis() - lastAlarmTime > 1000) {
      playWarningAlarm();
      lastAlarmTime = millis();
    }
  } else if (isHolding) {
    // 2. Jika Dahi Mochi Disentuh / Diusap
    drawHappyFace();
  } else {
    // 3. Tampilan Normal Berdasarkan Mode Layar
    switch (currentDisplayMode) {
      case 0: drawRidingMode(); break;
      case 1: drawClockMode(); break;
    }
  }

  delay(50);
}
