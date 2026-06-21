#include "DHT.h"

// =============================================
// ESP32 - SICAKLIK KONTROLLU FAN SISTEMI
// =============================================
// Bağlantılar (Resimdeki gibi):
// DHT11 (Mavi Isı Sensörü):
//   - VCC  -> 3.3V
//   - GND  -> GND
//   - DATA -> GPIO 4
//
// Transistör & Fan Sürücü:
//   - Direnç ucu -> GPIO 2 (D2 / G2 pini)
// =============================================

#define DHTPIN 4          // DHT11 Veri pini (Yeşil kablonun takılı olduğu 4 numaralı pin)
#define DHTTYPE DHT11

const int FAN_PIN = 2;    // Fan kontrol pini (GPIO 2 - dirençle transistöre bağlı)

// Sıcaklık Eşik Değerleri
const float TEMP_ON_THRESHOLD = 28.0;  // Fanın açılacağı sıcaklık (°C)
const float TEMP_OFF_THRESHOLD = 26.0; // Fanın kapanacağı sıcaklık (°C)

DHT dht(DHTPIN, DHTTYPE);
bool fanState = false;

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println(F("ESP32 Akıllı Fan Sistemi Başlatılıyor..."));
  
  // Sensör pinini dahili pull-up direnciyle başlat (Bare DHT11 için önemli)
  pinMode(DHTPIN, INPUT_PULLUP);
  dht.begin();
  
  // Transistör pinini çıkış yap
  pinMode(FAN_PIN, OUTPUT);
  digitalWrite(FAN_PIN, LOW); // Başlangıçta fan kapalı
}

void loop() {
  // Sıcaklık oku
  float temperature = dht.readTemperature();
  
  // Sensör okuma hatası kontrolü
  if (isnan(temperature)) {
    Serial.println(F("Hata: DHT11 sensöründen veri okunamıyor!"));
    delay(2000);
    return;
  }
  
  Serial.print(F("Sıcaklık: "));
  Serial.print(temperature);
  Serial.print(F(" °C -> "));

  // Fan kontrol mantığı
  if (temperature >= TEMP_ON_THRESHOLD && !fanState) {
    digitalWrite(FAN_PIN, HIGH); // Transistörü tetikle (Fanı aç)
    fanState = true;
    Serial.println(F("Fan AÇILDI (Sıcaklık yüksek)"));
  } 
  else if (temperature <= TEMP_OFF_THRESHOLD && fanState) {
    digitalWrite(FAN_PIN, LOW);  // Transistör tetiklemeyi kes (Fanı kapat)
    fanState = false;
    Serial.println(F("Fan KAPATILDI (Sınıf serinledi)"));
  } 
  else {
    if (fanState) {
      Serial.println(F("Fan çalışmaya devam ediyor..."));
    } else {
      Serial.println(F("Fan kapalı konumda bekliyor."));
    }
  }

  // 2 saniyede bir kontrol et
  delay(2000);
}
