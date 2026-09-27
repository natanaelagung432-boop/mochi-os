#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

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

// --- STATE SISTEM ---
bool deviceConnected = false;
String currentSpeed  = "0";
String currentMode   = "IDLE"; // Mode: IDLE, MAPS, RIDING, HAPPY, ANGRY, SLEEP, SURPRISED, CONFUSED
String currentDetail = "NORMAL";
String currentClock  = "08:45";

// --- STATE NAVIGASI MAPS ---
String navDirection = "LEFT";
String navDistance  = "150m";
String navStreet    = "Jl. Pemuda";
String navETA       = "10m";
String navTotalDist = "12.4km";

// --- STATE ANIMASI ORGANIK MOCHI ---
unsigned long lastBlinkTime    = 0;
unsigned long lastExpressionTime = 0;
bool isBlinking                = false;
int eyeXOffset                 = 0; 
int eyeYOffset                 = 0; 

// Enum Ekspresi Idle Dinamis
enum IdleExpression { EXPR_NORMAL, EXPR_SURPRISED, EXPR_CONFUSED, EXPR_YAWN };
IdleExpression currentIdleExpr = EXPR_NORMAL;

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
      currentMode = "HAPPY"; 
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      currentMode = "SLEEP"; 
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

// --- RENDER IKON NAVIGASI HUD (Area 50x50px) ---
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
    display.drawLine(18, 27, 36, 49, WHITE);
  } else if (dir == "SLIGHT_RIGHT") {
    display.fillTriangle(45, 22, 28, 17, 38, 34, WHITE);
    display.drawLine(33, 26, 15, 48, WHITE);
    display.drawLine(32, 27, 14, 49, WHITE);
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

  // 1. Header (Jam, Total Jarak, Status BLE)
  display.setCursor(0, 0);
  display.print(currentClock);

  if (navTotalDist.length() > 0) {
    display.setCursor(55, 0);
    display.print(navTotalDist);
  }

  display.setCursor(98, 0);
  display.print(deviceConnected ? "[BLE]" : "[OFF]");

  // Garis Pemisah Horizontal Header
  display.drawFastHLine(0, 10, 128, WHITE);

  // 2. Ikon Navigasi Sisi Kiri
  drawNavIcon(navDirection);

  // Garis Pemisah Vertikal (x = 51)
  display.drawFastVLine(51, 11, 53, WHITE);

  // 3. Teks Informasi Navigasi Sisi Kanan
  // Angka Jarak Belok (Teks Besar & Tebal)
  display.setTextSize(2);
  display.setCursor(56, 15);
  display.print(navDistance);

  // Nama Jalan (Auto Truncate jika panjang)
  display.setTextSize(1);
  display.setCursor(56, 35);
  String shortStreet = navStreet;
  if (shortStreet.length() > 11) {
    shortStreet = shortStreet.substring(0, 10) + ".";
  }
  display.print(shortStreet);

  // ETA / Estimasi Tiba
  display.setCursor(56, 51);
  display.print("ETA: " + navETA);
}

// --- ENGINE EKSPRESI HIDUP MOCHI ---
void drawLivingMochi() {
  unsigned long now = millis();

  // 1. TIMING BERKEDIP & MELIRIK OTOMATIS
  if (now - lastBlinkTime > 3500) {
    isBlinking = true;
    if (now - lastBlinkTime > 3650) {
      isBlinking = false;
      lastBlinkTime = now;
      eyeXOffset = random(-5, 6); // Melirik acak
      eyeYOffset = random(-2, 3);
    }
  }

  // 2. STATE MACHINE EKSPRESI IDLE BERBAGAI VARIASI (Tiap 12 detik ganti ekspresi acak)
  if (now - lastExpressionTime > 12000) {
    lastExpressionTime = now;
    int randVal = random(0, 10);
    if (randVal < 5)       currentIdleExpr = EXPR_NORMAL;
    else if (randVal < 7)  currentIdleExpr = EXPR_SURPRISED;
    else if (randVal < 9)  currentIdleExpr = EXPR_CONFUSED;
    else                   currentIdleExpr = EXPR_YAWN;
  }

  // --- RENDER VARIASI EKSPRESI ---

  // A. MODE MARAH / OVERSPEED / PANIC
  if (currentMode == "ANGRY" || currentDetail == "RIDE_PANIC") {
    // Alis Tajam Miring
    display.drawLine(20, 16, 48, 28, WHITE);
    display.drawLine(108, 16, 80, 28, WHITE);
    // Mata Tegas Oval
    display.fillRoundRect(24, 30, 22, 16, 4, WHITE);
    display.fillRoundRect(82, 30, 22, 16, 4, WHITE);
    // Mulut Segitiga Teriak
    display.fillTriangle(58, 46, 70, 46, 64, 56, WHITE);

    digitalWrite(BUZZER_PIN, HIGH);
    delay(20);
    digitalWrite(BUZZER_PIN, LOW);
  }

  // B. MODE SENANG (HAPPY / CONNECTED)
  else if (currentMode == "HAPPY") {
    // Mata Senyum Lengkung ^ ^
    display.drawCircle(35, 34, 12, WHITE);
    display.fillRect(20, 34, 30, 15, BLACK);
    display.drawCircle(93, 34, 12, WHITE);
    display.fillRect(78, 34, 30, 15, BLACK);
    // Mulut Terbuka Senang
    display.drawCircle(64, 44, 7, WHITE);
    display.fillRect(54, 36, 20, 9, BLACK);
  }

  // C. MODE SLEEP (DISCONNECTED)
  else if (currentMode == "SLEEP" || !deviceConnected) {
    // Mata Garis Terpejam u u
    display.fillRect(25, 36, 20, 3, WHITE);
    display.fillRect(83, 36, 20, 3, WHITE);
    // Teks Zzz Melayang
    display.setTextSize(1);
    display.setCursor(108, 18); display.print("z");
    display.setCursor(115, 10); display.print("Z");
  }

  // D. EKSPRESI IDLE DINAMIS
  else {
    // 1. KAGET / SURPRISED (Mata Bulat Besar)
    if (currentMode == "SURPRISED" || currentIdleExpr == EXPR_SURPRISED) {
      display.fillCircle(35, 35, 14, WHITE);
      display.fillCircle(93, 35, 14, WHITE);
      display.fillCircle(35, 35, 5, BLACK); // Pupil kecil
      display.fillCircle(93, 35, 5, BLACK);
      display.fillCircle(64, 48, 5, WHITE); // Mulut 'O' besar
      display.fillCircle(64, 48, 2, BLACK);
    }
    // 2. BINGUNG / CONFUSED (Mata Asimetris)
    else if (currentMode == "CONFUSED" || currentIdleExpr == EXPR_CONFUSED) {
      // Mata Kiri Normal, Mata Kanan Kecil
      display.fillRoundRect(25, 26, 20, 22, 6, WHITE);
      display.fillCircle(93, 35, 8, WHITE);
      // Alis Terangkat Sebelah
      display.drawLine(25, 20, 45, 18, WHITE);
      display.drawLine(83, 23, 103, 25, WHITE);
      // Mulut Miring
      display.drawLine(58, 48, 70, 45, WHITE);
    }
    // 3. MENGUAP / YAWN
    else if (currentIdleExpr == EXPR_YAWN) {
      // Mata Meram Sipit
      display.drawLine(25, 35, 45, 38, WHITE);
      display.drawLine(83, 38, 103, 35, WHITE);
      // Mulut Menguap Besar
      display.fillRoundRect(58, 42, 12, 16, 4, WHITE);
    }
    // 4. NORMAL IDLE (Berkedip, Melirik, Bernapas)
    else {
      if (isBlinking) {
        display.fillRect(25, 35, 22, 4, WHITE);
        display.fillRect(81, 35, 22, 4, WHITE);
      } else {
        // Mata Utama Mochi dengan Offset Melirik Dinamis
        int lx = 25 + eyeXOffset;
        int rx = 81 + eyeXOffset;
        int ey = 25 + eyeYOffset;

        display.fillRoundRect(lx, ey, 22, 24, 8, WHITE);
        display.fillRoundRect(rx, ey, 22, 24, 8, WHITE);
        // Kilatan Cahaya Piksel Mata
        display.fillRect(lx + 3, ey + 3, 6, 6, BLACK);
        display.fillRect(rx + 3, ey + 3, 6, 6, BLACK);
      }
      // Mulut Titik Imut
      display.fillCircle(64, 48, 3, WHITE);
    }
  }

  // Tampilan Kecepatan pada Riding Mode
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
    // Mode HUD Maps Navigasi
    renderMapsHUD();
  } else {
    // Mode Ekspresi Wajah Mochi Hidup
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
