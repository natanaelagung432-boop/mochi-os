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
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- KONFIGURASI SENSOR SENTUH (TOUCH SENSOR) ---
// Biasanya touch sensor terhubung ke GPIO 4, 12, 13, 14, atau 27
#define TOUCH_PIN 4        // Sesuaikan dengan pin sensor sentuh Mochi Anda
#define TOUCH_THRESHOLD 40 // Ambang batas sensitivitas sentuhan ESP32

// UUID Bluetooth
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
unsigned long lastDataTime = 0;
const long TIMEOUT_MS = 5000;

int currentSpeed = 0;
String currentMood = "IDLE";

// Callback Bluetooth Server
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
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

// --- FUNGSI MENGGAMBAR EKSPRESI ---

void drawIdleFace() {
  display.clearDisplay();
  display.fillRect(30, 24, 16, 16, SSD1306_WHITE);
  display.fillRect(82, 24, 16, 16, SSD1306_WHITE);
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
  // Pipi Merah / Love kecil saat disentuh
  display.drawLine(20, 36, 28, 36, SSD1306_WHITE);
  display.drawLine(100, 36, 108, 36, SSD1306_WHITE);
  display.display();
}

void drawSleepyFace() {
  display.clearDisplay();
  display.fillRect(25, 28, 20, 4, SSD1306_WHITE);
  display.fillRect(83, 28, 20, 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(105, 10);
  display.print("zZ");
  display.drawLine(58, 42, 70, 42, SSD1306_WHITE);
  display.display();
}

void drawSadFace() {
  display.clearDisplay();
  display.drawLine(25, 20, 35, 30, SSD1306_WHITE);
  display.drawLine(35, 30, 45, 20, SSD1306_WHITE);
  display.drawLine(83, 20, 93, 30, SSD1306_WHITE);
  display.drawLine(93, 30, 103, 20, SSD1306_WHITE);
  display.drawLine(58, 46, 64, 40, SSD1306_WHITE);
  display.drawLine(64, 40, 70, 46, SSD1306_WHITE);
  display.display();
}

void drawSpeedFace(int speed) {
  display.clearDisplay();
  display.drawLine(25, 20, 40, 30, SSD1306_WHITE);
  display.drawLine(25, 40, 40, 30, SSD1306_WHITE);
  display.drawLine(103, 20, 88, 30, SSD1306_WHITE);
  display.drawLine(103, 40, 88, 30, SSD1306_WHITE);
  
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(50, 22);
  display.print(speed);
  
  display.setTextSize(1);
  display.setCursor(52, 42);
  display.print("KM/H");
  display.display();
}

// Fungsi Pengecekan Sensor Sentuh
bool isTouched() {
  // Jika pakai Capacitive Touch bawaan ESP32:
  return (touchRead(TOUCH_PIN) < TOUCH_THRESHOLD);
  
  // Catatan: Jika Mochi Anda menggunakan sensor TTP223 (Digital HIGH/LOW),
  // ubah baris di atas menjadi: return (digitalRead(TOUCH_PIN) == HIGH);
}

void setup() {
  Serial.begin(115200);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Gagal menemukan OLED SSD1306"));
    for (;;);
  }
  display.clearDisplay();

  BLEDevice::init("DASAI_MOCHI");
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  BLEService *pService = pServer->createService(SERVICE_UUID);
  pCharacteristic = pService->createCharacteristic(
                      CHARACTERISTIC_UUIDWah, itu fitur bawaan Mochi yang **sangat bagus!** Sensor sentuh (*touch sensor*) hardware ini bekerja secara lokal di papan ESP32 tanpa memerlukan bantuan sinyal dari HP.

Sistem *stateless* yang baru saja kita simpan di file `firmware/mochi.ino` **sangat fleksibel** dan bisa kita gabungkan dengan sensor sentuh tersebut tanpa merusak koneksi Bluetooth!

---

### Cara Kerja Gabungannya:

1. **Kondisi Normal (GPS/Bluetooth HP):** Mochi menampilkan ekspresi sesuai kecepatan mobil atau tombol yang Anda tekan di Web App.
2. **Saat Disentuh (Touch Sensor Ditahan):** Mochi akan **memprioritaskan ekspresi BAHAGIA (`drawHappyFace`)** secara langsung.
3. **Saat Lepas Sentuhan:** Mochi akan kembali mengikuti data kecepatan dari HP/Web App secara otomatis.

---

### Penyesuaian Kode di `firmware/mochi.ino`

Agar sensor sentuh Anda kembali aktif dan bisa menimpa ekspresi wajah saat disentuh, buka file **`firmware/mochi.ino`** di GitHub Anda, klik ikon **Edit (pensil)**, lalu sesuaikan kodenya menjadi seperti berikut:

```cpp
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// Pin Sensor Sentuh (Umumnya di Pin T0/GPIO4 atau GPIO12, sesuaikan jika beda)
#define TOUCH_PIN 4 
#define TOUCH_THRESHOLD 40 // Nilai pembacaan sentuh (Makin kecil makin sensitif)

// Definisi Layar OLED 0.96 Inch
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// UUID Bluetooth
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

BLEServer* pServer = NULL;
BLECharacteristic* pCharacteristic = NULL;
bool deviceConnected = false;
unsigned long lastDataTime = 0;
const long TIMEOUT_MS = 5000;

int currentSpeed = 0;
String currentMood = "IDLE";

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
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
        int separator = rxValue.indexOf('|');
        if (separator != -1) {
          currentSpeed = rxValue.substring(0, separator).toInt();
          currentMood = rxValue.substring(separator + 1);
        }
      }
    }
};

// --- FUNGSI MENGGAMBAR EKSPRESI ---

void drawIdleFace() {
  display.clearDisplay();
  display.fillRect(30, 24, 16, 16, SSD1306_WHITE);
  display.fillRect(82, 24, 16, 16, SSD1306_WHITE);
  display.drawLine(59, 44, 69, 44, SSD1306_WHITE);
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

void drawSleepyFace() {
  display.clearDisplay();
  display.fillRect(25, 28, 20, 4, SSD1306_WHITE);
  display.fillRect(83, 28, 20, 4, SSD1306_WHITE);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(105, 10);
  display.print("zZ");
  display.drawLine(58, 42, 70, 42, SSD1306_WHITE);
  display.display();
}

void drawSadFace() {
  display.clearDisplay();
  display.drawLine(25, 20, 35, 30, SSD1306_WHITE);
  display.drawLine(35, 30, 45, 20, SSD1306_WHITE);
  display.drawLine(83, 20, 93, 30, SSD1306_WHITE);
  display.drawLine(93, 30, 103, 20, SSD1306_WHITE);
  display.drawLine(58, 46, 64, 40, SSD1306_WHITE);
  display.drawLine(64, 40, 70, 46, SSD1306_WHITE);
  display.display();
}

void drawSpeedFace(int speed) {
  display.clearDisplay();
  display.drawLine(25, 20, 40, 30, SSD1306_WHITE);
  display.drawLine(25, 40, 40, 30, SSD1306_WHITE);
  display.drawLine(103, 20, 88, 30, SSD1306_WHITE);
  display.drawLine(103, 40, 88, 30, SSD1306_WHITE);
  
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

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("Gagal menemukan OLED SSD1306"));
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
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  drawIdleFace();
}

void loop() {
  // 1. PRIORITAS UTAMA: Cek jika sensor sentuh disentuh/ditahan
  int touchValue = touchRead(TOUCH_PIN);
  
  if (touchValue < TOUCH_THRESHOLD) { 
    drawHappyFace(); // Tampilkan ekspresi bahagia saat disentuh
  } 
  // 2. Jika tidak disentuh, ikuti data Bluetooth dari HP
  else if (millis() - lastDataTime < TIMEOUT_MS && deviceConnected) {
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
  } 
  // 3. Jika tidak disentuh dan tidak ada sinyal HP, kembali ke IDLE
  else {
    drawIdleFace();
  }

  delay(100);
}
