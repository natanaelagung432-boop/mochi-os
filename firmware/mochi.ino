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

// --- UUID BLE (HARUS SAMA DENGAN INDEX.HTML) ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- VARIABLE STATE ---
bool deviceConnected = false;
String currentSpeed = "0";
String currentMode  = "NORMAL";
String currentDetail= "Melirik Kanan";
String currentClock = "00:00";

// --- CALLBACK BLE ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      // Restart advertising agar bisa terhubung kembali
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();

      if (rxValue.length() > 0) {
        // Parsing Payload: Kecepatan | Mode | Detail | Jam
        int firstPipe  = rxValue.indexOf('|');
        int secondPipe = rxValue.indexOf('|', firstPipe + 1);
        int thirdPipe  = rxValue.indexOf('|', secondPipe + 1);

        if (firstPipe != -1 && secondPipe != -1 && thirdPipe != -1) {
          currentSpeed  = rxValue.substring(0, firstPipe);
          currentMode   = rxValue.substring(firstPipe + 1, secondPipe);
          currentDetail = rxValue.substring(secondPipe + 1, thirdPipe);
          currentClock  = rxValue.substring(thirdPipe + 1);
        }
      }
    }
};

// --- FUNGSI GAMBAR MATA PIKSEL ---
void drawEyeNormal(int x, int y, int w, int h) {
  display.fillRoundRect(x, y, w, h, 6, SSD1306_WHITE);
}

void drawEyeHappy(int x, int y, int w, int h) {
  display.drawCircle(x + w/2, y + h/2, w/2, SSD1306_WHITE);
  display.fillRect(x, y + h/2, w + 2, h/2 + 2, SSD1306_BLACK);
}

void drawEyeHeart(int x, int y) {
  // Gambar bentuk Hati piksel sederhana
  display.fillCircle(x - 5, y - 5, 5, SSD1306_WHITE);
  display.fillCircle(x + 5, y - 5, 5, SSD1306_WHITE);
  display.fillTriangle(x - 10, y - 3, x + 10, y - 3, x, y + 10, SSD1306_WHITE);
}

void drawEyeSparkle(int x, int y) {
  // Bintang piksel
  display.fillTriangle(x, y - 10, x - 4, y + 4, x + 4, y + 4, SSD1306_WHITE);
  display.fillTriangle(x, y + 10, x - 4, y - 4, x + 4, y - 4, SSD1306_WHITE);
  display.fillTriangle(x - 10, y, x + 4, y - 4, x + 4, y + 4, SSD1306_WHITE);
  display.fillTriangle(x + 10, y, x - 4, y - 4, x - 4, y + 4, SSD1306_WHITE);
}

void drawArrowLeft() {
  display.fillTriangle(5, 32, 20, 18, 20, 46, SSD1306_WHITE);
  display.fillRect(20, 27, 10, 10, SSD1306_WHITE);
}

void drawArrowRight() {
  display.fillTriangle(123, 32, 108, 18, 108, 46, SSD1306_WHITE);
  display.fillRect(98, 27, 10, 10, SSD1306_WHITE);
}

void setup() {
  Serial.begin(115200);

  // Init Pin Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Init OLED Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Alamat I2C umum: 0x3C
    Serial.println(F("OLED Gagal Ditemukan!"));
    for (;;);
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(15, 25);
  display.println("Mochi Starting...");
  display.display();
  delay(1000);

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

  Serial.println("BLE Mochi Siap Terhubung!");
}

void loop() {
  display.clearDisplay();

  // --- HEADER: JAM & STATUS BLE ---
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(currentClock);

  display.setCursor(95, 0);
  if (deviceConnected) {
    display.print("[BLE]");
  } else {
    display.print("[OFF]");
  }

  // --- RENDER EXPRESI SESUAI DETAIL / MAPS ---
  
  // 1. MAPS MODE
  if (currentMode == "MAPS") {
    if (currentDetail == "MAPS_LEFT") {
      drawArrowLeft();
      drawEyeNormal(45, 20, 20, 30);
      drawEyeNormal(75, 20, 20, 30);
    } else if (currentDetail == "MAPS_RIGHT") {
      drawArrowRight();
      drawEyeNormal(35, 20, 20, 30);
      drawEyeNormal(65, 20, 20, 30);
    } else if (currentDetail == "MAPS_STRAIGHT") {
      drawEyeHappy(35, 20, 25, 25);
      drawEyeHappy(70, 20, 25, 25);
    }
  } 
  // 2. HAPPY MODE
  else if (currentMode == "HAPPY") {
    if (currentDetail == "HAPPY_L1") {
      drawEyeHappy(30, 20, 25, 25);
      drawEyeHappy(70, 20, 25, 25);
    } else if (currentDetail == "HAPPY_L2") {
      drawEyeHeart(40, 32);
      drawEyeHeart(88, 32);
    } else if (currentDetail == "HAPPY_L3") {
      drawEyeSparkle(40, 32);
      drawEyeSparkle(88, 32);
    }
  } 
  // 3. RIDING MODE
  else if (currentMode == "RIDING") {
    // Tampilkan Kecepatan di Tengah Bawah
    display.setCursor(45, 52);
    display.setTextSize(1);
    display.print(currentSpeed + " KM/H");

    if (currentDetail == "RIDE_PANIC") {
      // Overspeed Panic: Mata Melotot + Alarm Buzzer
      display.fillCircle(40, 28, 14, SSD1306_WHITE);
      display.fillCircle(88, 28, 14, SSD1306_WHITE);
      
      // Beep Buzzer Warning
      digitalWrite(BUZZER_PIN, HIGH);
      delay(50);
      digitalWrite(BUZZER_PIN, LOW);
    } else if (currentDetail == "RIDE_HAPPY") {
      drawEyeHappy(30, 18, 25, 25);
      drawEyeHappy(70, 18, 25, 25);
    } else { // RIDE_SANTAI
      drawEyeNormal(35, 20, 20, 25);
      drawEyeNormal(73, 20, 20, 25);
    }
  } 
  // 4. NORMAL MODE (GABUT)
  else {
    if (currentDetail.indexOf("Kanan") != -1) {
      drawEyeNormal(50, 20, 20, 30);
      drawEyeNormal(80, 20, 20, 30);
    } else if (currentDetail.indexOf("Kiri") != -1) {
      drawEyeNormal(30, 20, 20, 30);
      drawEyeNormal(60, 20, 20, 30);
    } else if (currentDetail.indexOf("Mengantuk") != -1) {
      display.fillRect(35, 30, 22, 6, SSD1306_WHITE);
      display.fillRect(71, 30, 22, 6, SSD1306_WHITE);
    } else if (currentDetail.indexOf("Wink") != -1) {
      display.fillRect(35, 32, 20, 4, SSD1306_WHITE); // Kedip
      drawEyeNormal(73, 20, 20, 30);
    } else {
      drawEyeNormal(35, 20, 20, 30);
      drawEyeNormal(73, 20, 20, 30);
    }
  }

  display.display();
  delay(100);
}
