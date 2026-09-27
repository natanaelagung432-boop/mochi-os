#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define TOUCH_PIN 4 
#define TOUCH_THRESHOLD 40 

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
const long TIMEOUT_MS = 5000;

// Variabel Data dari HP
int currentSpeed = 0;
String currentMood = "IDLE";
String currentTimeStr = "00:00";
String currentTempStr = "--C";
String currentWeatherStr = "CLEAR";

// Sistem Mode Layar (0: Riding, 1: Clock, 2: Notif, 3: Weather)
int currentDisplayMode = 0; 

// Variabel Deteksi Triple Tap
int tapCount = 0;
unsigned long lastTapTime = 0;
bool lastTouchState = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) { deviceConnected = true; };
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
        // Format: SPEED|MOOD|TIME|TEMP|WEATHER
        int p1 = rxValue.indexOf('|');
        int p2 = rxValue.indexOf('|', p1 + 1);
        int p3 = rxValue.indexOf('|', p2 + 1);
        int p4 = rxValue.indexOf('|', p3 + 1);

        if (p1 != -1) currentSpeed = rxValue.substring(0, p1).toInt();
        if (p2 != -1) currentMood = rxValue.substring(p1 + 1, p2);
        if (p3 != -1) currentTimeStr = rxValue.substring(p2 + 1, p3);
        if (p4 != -1) currentTempStr = rxValue.substring(p3 + 1, p4);
        if (p4 != -1) currentWeatherStr = rxValue.substring(p4 + 1);
      }
    }
};

// --- FUNGSI DESAIN MOCHI ---

void drawHappyFace() {
  display.clearDisplay();
  display.drawLine(25, 32, 35, 20, SSD1306_WHITE);
  display.drawLine(35, 20, 45, 32, SSD1306_WHITE);
  display.drawLine(83, 32, 93, 20, SSD1306_WHITE);
  display.drawLine(93, 20, 103, 32, SSD1306_WHITE);
  display.drawTriangle(58, 40, 70, 40, 64, 48, SSD1306_WHITE);
  display.display();
}

// 1. RIDING MODE
void drawRidingMode() {
  display.clearDisplay();
  if (currentSpeed > 60 || currentMood == "EXCITED") {
    // Tampilan Ngebut
    display.drawLine(25, 20, 40, 30, SSD1306_WHITE);
    display.drawLine(25, 40, 40, 30, SSD1306_WHITE);
    display.drawLine(103, 20, 88, 30, SSD1306_WHITE);
    display.drawLine(103, 40, 88, 30, SSD1306_WHITE);
  } else {
    // Wajah Santai Normal
    display.fillRect(30, 24, 16, 16, SSD1306_WHITE);
    display.fillRect(82, 24, 16, 16, SSD1306_WHITE);
  }
  // Speedometer di Tengah
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(50, 20);
  display.print(currentSpeed);
  display.setTextSize(1);
  display.setCursor(52, 40);
  display.print("KM/H");
  display.display();
}

// 2. CLOCK MODE
void drawClockMode() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 10);
  display.print("JAM SEKARANG");

  display.setTextSize(3);
  display.setCursor(20, 28);
  display.print(currentTimeStr); // Menampilkan Jam:Menit dari HP
  display.display();
}

// 3. NOTIFICATION MODE
void drawNotifMode() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(25, 10);
  display.print("--- NOTIFIKASI ---");

  if (deviceConnected) {
    display.setCursor(15, 30);
    display.print("Terhubung ke HP");
    display.setCursor(15, 45);
    display.print("Sistem Normal");
  } else {
    display.setCursor(15, 30);
    display.print("Terputus dari HP");
    display.setCursor(15, 45);
    display.print("Menunggu BLE...");
  }
  display.display();
}

// 4. WEATHER MODE
void drawWeatherMode() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 10);
  display.print("CUACA");

  display.setTextSize(2);
  display.setCursor(20, 30);
  display.print(currentTempStr); // Suhu

  display.setTextSize(1);
  display.setCursor(80, 35);
  display.print(currentWeatherStr); // Status Cuaca
  display.display();
}

// Fungsi Logika Pembacaan Triple Tap
void checkTouchLogic() {
  bool isTouching = (touchRead(TOUCH_PIN) < TOUCH_THRESHOLD);

  // Deteksi tepi sentuhan (Bukan ditahan)
  if (isTouching && !lastTouchState) {
    unsigned long now = millis();
    if (now - lastTapTime < 500) { // Jeda antarsentuhan maks 0.5 detik
      tapCount++;
    } else {
      tapCount = 1;
    }
    lastTapTime = now;
  }
  lastTouchState = isTouching;

  // Jika sudah 3x tap terdeteksi
  if (tapCount >= 3) {
    tapCount = 0;
    currentDisplayMode = (currentDisplayMode + 1) % 4; // Pindah Mode 0 -> 1 -> 2 -> 3 -> 0
  }
}

void setup() {
  Serial.begin(115200);

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

  drawRidingMode();
}

void loop() {
  checkTouchLogic();

  // Jika sensor ditahan terus (Hold), tampilkan Muka Bahagia
  bool isHolding = (touchRead(TOUCH_PIN) < TOUCH_THRESHOLD) && (tapCount < 2);

  if (isHolding) {
    drawHappyFace();
  } else {
    // Tampilkan Layar Berdasarkan Mode Terpilih
    switch (currentDisplayMode) {
      case 0: drawRidingMode(); break;
      case 1: drawClockMode(); break;
      case 2: drawNotifMode(); break;
      case 3: drawWeatherMode(); break;
    }
  }

  delay(50);
}
