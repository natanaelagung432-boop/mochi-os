#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#include "animasi.h"

// --- KONFIGURASI OLED ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- PIN BUZZER ---
#define BUZZER_PIN 23

// --- UUID BLE ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- STATE SISTEM & INTERAKSI ---
bool deviceConnected = false;
String currentSpeed  = "0";
String currentMode   = "IDLE"; // IDLE, MAPS, RIDING, HAPPY, ANGRY, SLEEP, DIZZY
String currentDetail = "NORMAL";
String currentClock  = "08:45";

// Status Navigasi Maps
String navDirection = "LEFT";
String navDistance  = "150m";
String navStreet    = "Jl. Pemuda";
String navETA       = "10m";
String navTotalDist = "12.4km";

// Variable Animasi Wajah Organik (Piksel Dinamis)
int currentFrameIndex = 0;
unsigned long lastEyeBlink = 0;
bool isBlinking = false;
int eyeXOffset = 0; // Efek melirik (-6 sampai 6)
int eyeYOffset = 0;

// --- PARSING DATA MAPS ---
void parseMapsData(String data) {
  data.trim();
  int c1 = data.indexOf(',');
  int c2 = data.indexOf(',', c1 + 1);
  int c3 = data.indexOf(',', c2 + 1);
  int c4 = data.indexOf(',', c3 + 1);

  if (c1 != -1 && c2 != -1 && c3 != -1) {
    navDirection = data.substring(0, c1);
    navDirection.trim();
    navDirection.toUpperCase();

    navDistance  = data.substring(c1 + 1, c2);
    navDistance.trim();

    navStreet    = data.substring(c2 + 1, c3);
    navStreet.trim();

    if (c4 != -1) {
      navETA       = data.substring(c3 + 1, c4);
      navETA.trim();
      navTotalDist = data.substring(c4 + 1);
      navTotalDist.trim();
    } else {
      navETA       = data.substring(c3 + 1);
      navETA.trim();
      navTotalDist = "";
    }
  }
}

// --- CALLBACK BLE ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      currentMode = "HAPPY"; // Mochi gembira saat HP terhubung!
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      currentMode = "SLEEP"; // Mochi tertidur jika terputus
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();
      rxValue.trim();

      if (rxValue.length() > 0) {
        int firstPipe  = rxValue.indexOf('|');
        int secondPipe = rxValue.indexOf('|', firstPipe + 1);
        int thirdPipe  = rxValue.indexOf('|', secondPipe + 1);

        if (firstPipe != -1 && secondPipe != -1 && thirdPipe != -1) {
          currentSpeed  = rxValue.substring(0, firstPipe);
          currentMode   = rxValue.substring(firstPipe + 1, secondPipe);
          currentDetail = rxValue.substring(secondPipe + 1, thirdPipe);
          currentClock  = rxValue.substring(thirdPipe + 1);

          currentMode.trim();
          currentMode.toUpperCase();

          if (currentMode == "MAPS") {
            parseMapsData(currentDetail);
          }
        }
      }
    }
};

// --- RENDER IKON NAVIGASI HUD ---
void drawNavIcon(String dir) {
  if (dir == "LEFT") {
    display.fillTriangle(5, 36, 25, 20, 25, 52, WHITE);
    display.fillRect(25, 31, 20, 10, WHITE);
  } else if (dir == "RIGHT") {
    display.fillTriangle(45, 36, 25, 20, 25, 52, WHITE);
    display.fillRect(5, 31, 20, 10, WHITE);
  } else if (dir == "SLIGHT_LEFT") {
    display.fillTriangle(5, 22, 22, 17, 12, 34, WHITE);
    display.drawLine(17, 26, 35, 48, WHITE);
  } else if (dir == "SLIGHT_RIGHT") {
    display.fillTriangle(45, 22, 28, 17, 38, 34, WHITE);
    display.drawLine(33, 26, 15, 48, WHITE);
  } else if (dir == "UTURN") {
    display.drawCircle(25, 28, 14, WHITE);
    display.fillRect(11, 28, 6, 20, WHITE);
    display.fillRect(33, 28, 6, 10, WHITE);
    display.fillTriangle(8, 48, 20, 48, 14, 55, WHITE);
  } else { // STRAIGHT
    display.fillTriangle(25, 16, 10, 33, 40, 33, WHITE);
    display.fillRect(20, 33, 10, 20, WHITE);
  }
}

// --- RENDER LAYAR MAPS HUD ---
void renderMapsHUD() {
  display.setTextSize(1);
  display.setTextColor(WHITE);

  display.setCursor(0, 0);
  display.print(currentClock);

  if (navTotalDist.length() > 0) {
    display.setCursor(60, 0);
    display.print(navTotalDist);
  }

  display.setCursor(100, 0);
  display.print(deviceConnected ? "[BLE]" : "[OFF]");

  display.drawFastHLine(0, 10, 128, WHITE);
  drawNavIcon(navDirection);
  display.drawFastVLine(51, 11, 53, WHITE);

  display.setTextSize(2);
  display.setCursor(56, 16);
  display.print(navDistance);

  display.setTextSize(1);
  display.setCursor(56, 36);
  String shortStreet = navStreet;
  if (shortStreet.length() > 11) {
    shortStreet = shortStreet.substring(0, 10) + ".";
  }
  display.print(shortStreet);

  display.setCursor(56, 52);
  display.print("ETA: " + navETA);
}

// --- LOGIKA WAJAH MOCHI INTERAKTIF & HIDUP ---
void drawLivingMochi() {
  // Update efek berkedip otomatis
  if (millis() - lastEyeBlink > 3000) {
    isBlinking = true;
    if (millis() - lastEyeBlink > 3150) {
      isBlinking = false;
      lastEyeBlink = millis();
      // Acak arah melirik mata setiap selesai berkedip
      eyeXOffset = random(-4, 5); 
    }
  }

  // 1. MODE ANGRY (Jika Kecepatan Terlalu Tinggi / Overspeed)
  if (currentMode == "ANGRY" || currentDetail == "RIDE_PANIC") {
    // Alis Marah / Galak
    display.drawLine(20, 18, 50, 30, WHITE);
    display.drawLine(108, 18, 78, 30, WHITE);
    // Mata Tegas
    display.fillRoundRect(25, 32, 20, 15, 4, WHITE);
    display.fillRoundRect(83, 32, 20, 15, 4, WHITE);
    // Mulut Teriak Segitiga
    display.fillTriangle(58, 48, 70, 48, 64, 58, WHITE);

    // Bip Buzzer peringatan
    digitalWrite(BUZZER_PIN, HIGH);
    delay(20);
    digitalWrite(BUZZER_PIN, LOW);
  }
  
  // 2. MODE HAPPY (Menyapa saat terhubung BLE)
  else if (currentMode == "HAPPY") {
    // Mata Lengkung Senang ^ ^
    display.drawCircle(35, 35, 12, WHITE);
    display.fillRect(20, 35, 30, 15, BLACK); // Potong bawah
    display.drawCircle(93, 35, 12, WHITE);
    display.fillRect(78, 35, 30, 15, BLACK);
    // Mulut Senyum
    display.drawCircle(64, 45, 8, WHITE);
    display.fillRect(54, 37, 20, 10, BLACK);
  }

  // 3. MODE SLEEP (Saat Bluetooth Terputus)
  else if (currentMode == "SLEEP" || !deviceConnected) {
    // Mata Terpejam u u
    display.drawLine(25, 38, 45, 38, WHITE);
    display.drawLine(83, 38, 103, 38, WHITE);
    // Teks Zzz
    display.setTextSize(1);
    display.setCursor(110, 15);
    display.print("z");
    display.setCursor(116, 8);
    display.print("Z");
  }

  // 4. MODE IDLE / NORMAL (Mochi Bernapas, Berkedip & Melirik)
  else {
    if (isBlinking) {
      // Garis Berkedip
      display.fillRect(25, 35, 22, 4, WHITE);
      display.fillRect(81, 35, 22, 4, WHITE);
    } else {
      // Mata Bulat Mochi dengan Efek Melirik Dinamis
      display.fillRoundRect(25 + eyeXOffset, 25, 22, 24, 8, WHITE);
      display.fillRoundRect(81 + eyeXOffset, 25, 22, 24, 8, WHITE);
      // Kilatan Cahaya Piksel di Mata (Pupil)
      display.fillRect(28 + eyeXOffset, 28, 6, 6, BLACK);
      display.fillRect(84 + eyeXOffset, 28, 6, 6, BLACK);
    }
    // Mulut Mochi Imut (O-shape / Dot)
    display.fillCircle(64, 48, 3, WHITE);
  }

  // Tampilkan Kecepatan di bagian bawah jika dalam Mode Riding
  if (currentMode == "RIDING") {
    display.setTextSize(1);
    display.setCursor(42, 55);
    display.print(currentSpeed + " KM/H");
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(15, 25);
  display.print("Mochi Starting...");
  display.display();
  delay(1000);

  parseMapsData(currentDetail);

  // Init BLE
  BLEDevice::init("Mochi-ESP32");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
                                         CHARACTERISTIC_UUID,
                                         BLECharacteristic::PROPERTY_WRITE
                                       );

  pCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  BLEDevice::startAdvertising();
}

void loop() {
  display.clearDisplay();

  if (currentMode == "MAPS") {
    renderMapsHUD();
  } else {
    drawLivingMochi();

    // Top Header Overlay
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(currentClock);
    display.setCursor(98, 0);
    display.print(deviceConnected ? "[BLE]" : "[OFF]");
  }

  display.display();
  delay(50);
}
