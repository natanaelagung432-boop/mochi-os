#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Definisi Layar OLED 0.96 Inch
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C // Alamat I2C umum OLED 0.96"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// UUID Bluetooth (Harus sama persis dengan index.html)
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
unsigned long lastDataTime = 0;
const long TIMEOUT_MS = 5000; // Kembalikan ke IDLE jika 5 detik tidak ada sinyal HP

int currentSpeed = 0;
String currentMood = "IDLE";

// Callback Bluetooth Server
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      // Mulai ulang advertising agar bisa dikoneksikan kembali
      BLEDevice::startAdvertising();
    }
};

// Callback Bluetooth Data Masuk
class MyCallbacks: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      String rxValue = pCharacteristic->getValue().c_str();
      if (rxValue.length() > 0) {
        lastDataTime = millis();
        int separator = rxValue.indexOf('|');
        if (separator != -1) {
          currentSpeed = rxValue.substring(0, separator).toInt();
          currentMood = rxValue.substring(separator + 1);
        }
      }
    }
};

// --- FUNGSI MENGGAMBAR EKSPRESI MOCHI ---

void drawIdleFace() {
  display.clearDisplay();
  // Mata Kiri (KEDIP/BIASA)
  display.fillRect(30, 24, 16, 16, SSD1306_WHITE);
  // Mata Kanan
  display.fillRect(82, 24, 16, 16, SSD1306_WHITE);
  // Mulut Kecil Datar
  display.drawLine(59, 44, 69, 44, SSD1306_WHITE);
  display.display();
}

void drawHappyFace() {
  display.clearDisplay();
  // Mata Kiri Lengkung Atas ( ^ )
  display.drawLine(25, 32, 35, 20, SSD1306_WHITE);
  display.drawLine(35, 20, 45, 32, SSD1306_WHITE);
  // Mata Kanan Lengkung Atas ( ^ )
  display.drawLine(83, 32, 93, 20, SSD1306_WHITE);
  display.drawLine(93, 20, 103, 32, SSD1306_WHITE);
  // Mulut Senyum Segitiga
  display.drawTriangle(58, 40, 70, 40, 64, 48, SSD1306_WHITE);
  display.display();
}

void drawSleepyFace() {
  display.clearDisplay();
  // Mata Garis Horizontal
  display.fillRect(25, 28, 20, 4, SSD1306_WHITE);
  display.fillRect(83, 28, 20, 4, SSD1306_WHITE);
  // Tulisan "zZ"
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(105, 10);
  display.print("zZ");
  // Mulut Datar
  display.drawLine(58, 42, 70, 42, SSD1306_WHITE);
  display.display();
}

void drawSadFace() {
  display.clearDisplay();
  // Mata Lengkung Bawah ( U )
  display.drawLine(25, 20, 35, 30, SSD1306_WHITE);
  display.drawLine(35, 30, 45, 20, SSD1306_WHITE);
  display.drawLine(83, 20, 93, 30, SSD1306_WHITE);
  display.drawLine(93, 30, 103, 20, SSD1306_WHITE);
  // Mulut Cemberut
  display.drawLine(58, 46, 64, 40, SSD1306_WHITE);
  display.drawLine(64, 40, 70, 46, SSD1306_WHITE);
  display.display();
}

void drawSpeedFace(int speed) {
  display.clearDisplay();
  // Mata > < (Ngebut / Excited)
  display.drawLine(25, 20, 40, 30, SSD1306_WHITE);
  display.drawLine(25, 40, 40, 30, SSD1306_WHITE);
  display.drawLine(103, 20, 88, 30, SSD1306_WHITE);
  display.drawLine(103, 40, 88, 30, SSD1306_WHITE);
  
  // Tampilkan Angka Kecepatan di Tengah
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(50, 22);
  display.print(speed);
  
  display.setTextSize(1);
  display.setCursor(52, 42);
  display.print("KM/H");
  display.display();
}

void setup() {
  Serial.begin(115200);

  // Inisialisasi Layar OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Gagal menemukan OLED SSD1306"));
    for (;;); // Stop jika OLED tidak terbaca
  }
  display.clearDisplay();

  // Inisialisasi Bluetooth BLE
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
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  drawIdleFace();
}

void loop() {
  // Cek apakah data dari HP masih segar (kurang dari 5 detik lalu)
  if (millis() - lastDataTime < TIMEOUT_MS && deviceConnected) {
    if (currentMood == "HAPPY") {
      drawHappyFace();
    } else if (currentMood == "SLEEPY") {
      drawSleepyFace();
    } else if (currentMood == "SAD") {
      drawSadFace();
    } else if (currentMood == "EXCITED" || currentSpeed > 60) {
      drawSpeedFace(currentSpeed);
    } else {
      drawIdleFace();
    }
  } else {
    drawIdleFace(); // Kembali ke wajah netral jika koneksi putus/idle
  }

  delay(100); // Refresh rate halus
}
