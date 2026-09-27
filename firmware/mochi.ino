#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Include file animasi bitmap
#include "animasi.h"

// --- KONFIGURASI OLED (U8g2) ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- PIN BUZZER ---
#define BUZZER_PIN 23

// --- UUID BLE ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- VARIABLE STATE ---
bool deviceConnected = false;
String currentSpeed  = "0";
String currentMode   = "MAPS"; // Default diset ke MAPS agar langsung terlihat
String currentDetail = "LEFT,150m,Jl. Pemuda,10m,12.4km";
String currentClock  = "08:45";

// Variable Navigasi Maps
String navDirection = "LEFT";
String navDistance  = "150m";
String navStreet    = "Jl. Pemuda";
String navETA       = "10m";
String navTotalDist = "12.4km";

// Frame tracker animasi
int currentFrameIndex = 0;

// --- FUNGSI PARSING DATA MAPS ---
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
      Serial.println("[BLE] Device Terhubung!");
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      Serial.println("[BLE] Device Terputus! Re-advertising...");
      BLEDevice::startAdvertising();
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();
      rxValue.trim();

      if (rxValue.length() > 0) {
        Serial.print("[BLE Data Masuk]: ");
        Serial.println(rxValue);

        // Parsing Payload: Kecepatan | Mode | Detail | Jam
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

// --- FUNGSI MENGGAMBAR IKON PANAH NAVIGASI ---
void drawNavIcon(String dir) {
  if (dir == "LEFT") {
    u8g2.drawTriangle(5, 36, 25, 20, 25, 52);
    u8g2.drawBox(25, 31, 20, 10);
  } 
  else if (dir == "RIGHT") {
    u8g2.drawTriangle(45, 36, 25, 20, 25, 52);
    u8g2.drawBox(5, 31, 20, 10);
  } 
  else if (dir == "SLIGHT_LEFT") {
    u8g2.drawTriangle(5, 22, 22, 17, 12, 34);
    u8g2.drawLine(17, 26, 35, 48);
    u8g2.drawLine(18, 27, 36, 49);
  } 
  else if (dir == "SLIGHT_RIGHT") {
    u8g2.drawTriangle(45, 22, 28, 17, 38, 34);
    u8g2.drawLine(33, 26, 15, 48);
    u8g2.drawLine(32, 27, 14, 49);
  } 
  else if (dir == "UTURN") {
    u8g2.drawCircle(25, 28, 14, U8G2_DRAW_UPPER_LEFT | U8G2_DRAW_UPPER_RIGHT);
    u8g2.drawBox(11, 28, 6, 20);
    u8g2.drawBox(33, 28, 6, 10);
    u8g2.drawTriangle(8, 48, 20, 48, 14, 55);
  } 
  else { // STRAIGHT / DEFAULT
    u8g2.drawTriangle(25, 16, 10, 33, 40, 33);
    u8g2.drawBox(20, 33, 10, 20);
  }
}

// --- FUNGSI RENDER TAMPILAN MAPS HUD ---
void renderMapsHUD() {
  // 1. Header (Info Jam, Total Jarak, dan Status BLE)
  u8g2.setFont(u8g2_font_micro_tr);
  u8g2.drawStr(0, 8, currentClock.c_str());
  
  if (navTotalDist.length() > 0) {
    u8g2.drawStr(60, 8, navTotalDist.c_str());
  }
  
  if (deviceConnected) {
    u8g2.drawStr(104, 8, "[BLE]");
  } else {
    u8g2.drawStr(104, 8, "[OFF]");
  }

  // Garis Pemisah Header (Horizontal)
  u8g2.drawHLine(0, 10, 128);

  // 2. Ikon Arah Utama (Area Kiri: x=0..50)
  drawNavIcon(navDirection);

  // Garis Pemisah Vertikal
  u8g2.drawVLine(51, 11, 53);

  // 3. Teks Informasi Navigasi (Area Kanan: x=54..128)
  // Jarak ke Belokan
  u8g2.setFont(u8g2_font_7x14B_tr);
  u8g2.drawStr(55, 26, navDistance.c_str());

  // Nama Jalan
  u8g2.setFont(u8g2_font_6x10_tr);
  String shortStreet = navStreet;
  if (shortStreet.length() > 11) {
    shortStreet = shortStreet.substring(0, 10) + ".";
  }
  u8g2.drawStr(55, 41, shortStreet.c_str());

  // Estimasi Tiba (ETA)
  u8g2.setFont(u8g2_font_micro_tr);
  String etaText = "ETA: " + navETA;
  u8g2.drawStr(55, 58, etaText.c_str());
}

void setup() {
  Serial.begin(115200);

  // Init Pin Buzzer
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  // Init OLED Display dengan U8g2
  u8g2.begin();
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  u8g2.drawStr(15, 35, "Mochi Starting...");
  u8g2.sendBuffer();
  delay(1000);

  // Parse data default awal
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

  Serial.println("BLE Mochi Navigasi Siap!");
}

void loop() {
  u8g2.clearBuffer();

  // EVALUASI MODE
  if (currentMode == "MAPS") {
    // Mode Turn-by-Turn HUD Maps
    renderMapsHUD();
  } 
  else {
    // Mode Animasi Bitmap
    #if defined(ANIMATION_H) || defined(ANIM_WIDTH)
      const unsigned char* framePtr = (const unsigned char*)pgm_read_ptr(&(frames[currentFrameIndex]));
      u8g2.drawXBMP(0, 0, ANIM_WIDTH, ANIM_HEIGHT, framePtr);
      currentFrameIndex = (currentFrameIndex + 1) % totalFrames;
    #else
      u8g2.setFont(u8g2_font_6x10_tr);
      u8g2.drawStr(30, 35, "MOCHI IDLE");
    #endif

    // Header Overlay
    u8g2.setFont(u8g2_font_profont10_tf);
    u8g2.drawStr(0, 8, currentClock.c_str());
    if (deviceConnected) u8g2.drawStr(98, 8, "[BLE]");
    
    // Fitur Speedometer jika Riding
    if (currentMode == "RIDING") {
      String speedText = currentSpeed + " KM/H";
      u8g2.drawStr(45, 60, speedText.c_str());

      if (currentDetail == "RIDE_PANIC") {
        digitalWrite(BUZZER_PIN, HIGH);
        delay(30);
        digitalWrite(BUZZER_PIN, LOW);
      }
    }
  }

  // Kirim Buffer ke OLED
  u8g2.sendBuffer();
  
  delay(50);
}
