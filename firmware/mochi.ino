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
// Menggunakan driver SSD1306 128x64 I2C
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// --- PIN BUZZER ---
#define BUZZER_PIN 23

// --- UUID BLE ---
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

// --- VARIABLE STATE ---
bool deviceConnected = false;
String currentSpeed = "0";
String currentMode  = "NORMAL";
String currentDetail= "Melirik Kanan";
String currentClock = "00:00";

// Frame tracker untuk animasi
int currentFrameIndex = 0;

// --- CALLBACK BLE ---
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      BLEDevice::startAdvertising(); // Restart advertising agar bisa terhubung kembali
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
  u8g2.clearBuffer();

  // 1. RENDER FRAME ANIMASI DARI animasi.h
  // Membaca pointer bitmap dari PROGMEM
  const unsigned char* framePtr = (const unsigned char*)pgm_read_ptr(&(frames[currentFrameIndex]));
  u8g2.drawXBMP(0, 0, ANIM_WIDTH, ANIM_HEIGHT, framePtr);

  // Iterasi ke frame berikutnya
  currentFrameIndex = (currentFrameIndex + 1) % totalFrames;

  // 2. HEADER OVERLAY: JAM & STATUS BLE
  u8g2.setFont(u8g2_font_profont10_tf);
  u8g2.drawStr(0, 8, currentClock.c_str());

  if (deviceConnected) {
    u8g2.drawStr(98, 8, "[BLE]");
  } else {
    u8g2.drawStr(98, 8, "[OFF]");
  }

  // 3. LOGIKA TAMBAHAN SESUAI MODE BLE (misal: Buzzer alarm & Info Kecepatan)
  if (currentMode == "RIDING") {
    // Tampilkan Kecepatan di Bawah
    String speedText = currentSpeed + " KM/H";
    u8g2.drawStr(45, 60, speedText.c_str());

    if (currentDetail == "RIDE_PANIC") {
      // Alarm Beep Warning saat Panic/Overspeed
      digitalWrite(BUZZER_PIN, HIGH);
      delay(30);
      digitalWrite(BUZZER_PIN, LOW);
    }
  }

  // Kirim Buffer ke Layar Display OLED
  u8g2.sendBuffer();
  
  // Delay antar frame animasi
  delay(50);
}
