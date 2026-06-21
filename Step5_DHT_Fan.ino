#include <IRremote.hpp>
#include <MFRC522.h>
#include <SPI.h>
#include <Servo.h>
#include <DHT.h> // YENI: Sicaklik sensoru kutuphanesi

// RC522
// SDA(SS) -> D10
// SCK     -> D13
// MOSI    -> D11
// MISO    -> D12
// RST     -> D8
//
// Door servo
// Signal  -> D6
//
// PIR
// OUT     -> D2
//
// Light output
// IN      -> A2
//
// IR Receiver
// OUT/S   -> A5
//
// Screen Servo
// Signal  -> D3
//
// DHT11 Sensor (YENI)
// OUT/S   -> A0
// VCC     -> 5V
// GND     -> GND
//
// Fan/Havalandirma (YENI)
// IN1     -> D4
// IN2     -> D9
// VCC     -> 5V
// GND     -> GND

const byte RFID_SS_PIN = 10;
const byte RFID_RST_PIN = 9; // RFID RST 9'a takilmis
const byte DOOR_SERVO_PIN = 6;
const byte PIR_PIN = 2;
const byte LIGHT_PIN = A2;
const byte IR_RECEIVER_PIN = A5;
const byte SCREEN_SERVO_PIN = 3;

// YENI PINLER (DHT11 ve Fan)
const byte DHT_PIN = A4; // Kullanici A4'e takmis
const byte FAN_IN1_PIN = 7;
const byte FAN_IN2_PIN = A1;

#define DHT_TYPE DHT11

// Kapi ve Sure Ayarlari
const byte DOOR_LOCKED_ANGLE = 0;
const byte DOOR_UNLOCKED_ANGLE = 90;
const unsigned long DOOR_OPEN_TIME_MS = 7000;
const unsigned long PIR_WARMUP_TIME_MS = 30000;
const unsigned long ROOM_EMPTY_TIMEOUT_MS = 15000; // Test icin 15 sn

// Projeksiyon Perdesi
const byte SCREEN_UP_ANGLE = 20;
const byte SCREEN_DOWN_ANGLE = 150;

// Kumanda Sifreleri
const IRRawDataType IR_CODE_SCREEN_UP = 0xE718FF00;
const IRRawDataType IR_CODE_SCREEN_DOWN = 0xAD52FF00;

// Sicaklik Esik Degerleri (YENI)
const float FAN_ON_TEMPERATURE = 10.0;  // Test icin cok dusuruldu (Oda sicakliginin altina)
const float FAN_OFF_TEMPERATURE = 5.0;  // Test icin
const unsigned long DHT_READ_INTERVAL_MS = 2000; // 2 saniyede bir olcum

const byte LIGHT_ON_LEVEL = HIGH;
const byte LIGHT_OFF_LEVEL = LOW;
const byte PIR_ACTIVE_LEVEL = HIGH;
const byte FAN_ON_LEVEL = HIGH;  // Role ters calisiyorsa LOW yapin
const byte FAN_OFF_LEVEL = LOW;

struct AuthorizedCard {
  byte uid[10];
  byte size;
};

const AuthorizedCard AUTHORIZED_CARDS[] = {
    {{0x15, 0x47, 0x1F, 0x07}, 4}, 
    {{0xEB, 0x18, 0xF0, 0x06}, 4}, 
};

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo doorServo;
Servo screenServo;
DHT dht(DHT_PIN, DHT_TYPE); // YENI: DHT Sensor objesi

bool roomOccupied = false;
bool doorIsOpen = false;
bool lightIsOn = false;
bool pirWasActive = false;
bool fanIsOn = false; // YENI: Fan durumu

byte currentScreenAngle = 255;
unsigned long doorOpenAt = 0;
unsigned long lastMotionAt = 0;
unsigned long pirWarmupStartedAt = 0;
unsigned long lastDhtReadAt = 0; // YENI: Son okuma zamani

bool isAuthorizedCard(const MFRC522::Uid &uid);
void printUid(const MFRC522::Uid &uid);
void openDoor(unsigned long now);
void closeDoor();
void setLight(bool enabled);
void setScreenAngle(byte angle);
void handleIrRemote();

// YENI FONKSIYONLAR
void setFan(bool enabled);
void handleClimate(unsigned long now);

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(FAN_IN1_PIN, OUTPUT);
  pinMode(FAN_IN2_PIN, OUTPUT);

  SPI.begin();
  rfid.PCD_Init();
  delay(5);
  rfid.PCD_AntennaOn();
  rfid.PCD_SetAntennaGain(MFRC522::RxGain_max);

  doorServo.attach(DOOR_SERVO_PIN);
  closeDoor();

  screenServo.attach(SCREEN_SERVO_PIN);
  setScreenAngle(SCREEN_UP_ANGLE);

  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK);
  
  dht.begin(); // YENI: Sensoru baslat

  setLight(false);
  setFan(false); // YENI: Fani kapali baslat
  pirWarmupStartedAt = millis();

  Serial.println(F("---- STEP 5 READY ----"));
  Serial.println(F("DHT11 Sicaklik Sensoru ve Fan eklendi."));
  Serial.println(F("Oda doluysa ve sicaklik 28'i gecerse fan otomatik acilacak."));
}

void loop() {
  unsigned long now = millis();
  bool pirActive = digitalRead(PIR_PIN) == PIR_ACTIVE_LEVEL;

  handleIrRemote();
  
  // YENI: Sicaklik kontrolu
  handleClimate(now);

  if (doorIsOpen && now - doorOpenAt >= DOOR_OPEN_TIME_MS) {
    closeDoor();
    Serial.println(F("Door locked again."));
  }

  if (pirActive && !pirWasActive) {
    Serial.println(F("Motion detected by PIR."));
  }

  if (!pirActive && pirWasActive) {
    Serial.println(F("PIR returned to idle."));
  }

  pirWasActive = pirActive;

  if (roomOccupied && now - pirWarmupStartedAt >= PIR_WARMUP_TIME_MS &&
      pirActive) {
    lastMotionAt = now;
  }

  if (roomOccupied && now - lastMotionAt >= ROOM_EMPTY_TIMEOUT_MS) {
    roomOccupied = false;
    setLight(false);
    setFan(false); // YENI: Oda bosalirsa fani da kapat
    Serial.println(F("No motion detected. Room marked empty (Lights & Fan OFF)."));
  }

  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print(F("Card UID: "));
  printUid(rfid.uid);

  if (isAuthorizedCard(rfid.uid)) {
    Serial.println(F(" -> Authorized"));

    if (!doorIsOpen) {
      openDoor(now);
    } else {
      Serial.println(F("Door is already open."));
    }

    roomOccupied = true;
    lastMotionAt = now;
    setLight(true);
  } else {
    Serial.println(F(" -> Unauthorized"));
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(700);
}

// YENI FONKSIYON: Sicakligi olcer ve fani yonetir
void handleClimate(unsigned long now) {
  // 2 saniyede bir olcum yap (DHT11 yavas bir sensordur)
  if (now - lastDhtReadAt < DHT_READ_INTERVAL_MS) {
    return;
  }
  lastDhtReadAt = now;

  float temp = dht.readTemperature();

  // Okuma basarisizsa cik
  if (isnan(temp)) {
    Serial.println(F("Hata: DHT11 okunamadi!"));
    return;
  }

  // Eger oda bossa fan calismasina gerek yok
  if (!roomOccupied) {
    if (fanIsOn) setFan(false);
    return;
  }

  // Oda doluysa sicakliga gore fani kontrol et
  if (!fanIsOn && temp >= FAN_ON_TEMPERATURE) {
    setFan(true);
    Serial.print(F("Sicaklik yuksek ("));
    Serial.print(temp);
    Serial.println(F("C). Fan ACILDI."));
  } else if (fanIsOn && temp <= FAN_OFF_TEMPERATURE) {
    setFan(false);
    Serial.print(F("Sicaklik normal ("));
    Serial.print(temp);
    Serial.println(F("C). Fan KAPATILDI."));
  }
}

// YENI FONKSIYON: Fani acar kapatir
void setFan(bool enabled) {
  if (enabled) {
    digitalWrite(FAN_IN1_PIN, HIGH);
    digitalWrite(FAN_IN2_PIN, LOW);
  } else {
    digitalWrite(FAN_IN1_PIN, LOW);
    digitalWrite(FAN_IN2_PIN, LOW);
  }
  fanIsOn = enabled;
}

void setScreenAngle(byte angle) {
  if (currentScreenAngle == angle) {
    return; 
  }
  screenServo.write(angle);
  currentScreenAngle = angle;
}

void handleIrRemote() {
  if (!IrReceiver.decode()) {
    return; 
  }

  const IRRawDataType rawCode = IrReceiver.decodedIRData.decodedRawData;

  if (rawCode == IR_CODE_SCREEN_UP) {
    setScreenAngle(SCREEN_UP_ANGLE);
    Serial.println(F("Projeksiyon perdesi YUKARI kalkti."));
  } else if (rawCode == IR_CODE_SCREEN_DOWN) {
    setScreenAngle(SCREEN_DOWN_ANGLE);
    Serial.println(F("Projeksiyon perdesi ASAGI indi."));
  }
  IrReceiver.resume();
}

bool isAuthorizedCard(const MFRC522::Uid &uid) {
  for (size_t i = 0; i < sizeof(AUTHORIZED_CARDS) / sizeof(AUTHORIZED_CARDS[0]);
       ++i) {
    if (uid.size != AUTHORIZED_CARDS[i].size) {
      continue;
    }

    bool matched = true;
    for (byte j = 0; j < uid.size; ++j) {
      if (uid.uidByte[j] != AUTHORIZED_CARDS[i].uid[j]) {
        matched = false;
        break;
      }
    }

    if (matched) {
      return true;
    }
  }

  return false;
}

void printUid(const MFRC522::Uid &uid) {
  for (byte i = 0; i < uid.size; ++i) {
    if (uid.uidByte[i] < 0x10) {
      Serial.print('0');
    }
    Serial.print(uid.uidByte[i], HEX);
    if (i + 1 < uid.size) {
      Serial.print(' ');
    }
  }
}

void openDoor(unsigned long now) {
  doorServo.write(DOOR_UNLOCKED_ANGLE);
  doorIsOpen = true;
  doorOpenAt = now;
  Serial.println(F("Door unlocked."));
}

void closeDoor() {
  doorServo.write(DOOR_LOCKED_ANGLE);
  doorIsOpen = false;
}

void setLight(bool enabled) {
  digitalWrite(LIGHT_PIN, enabled ? LIGHT_ON_LEVEL : LIGHT_OFF_LEVEL);
  lightIsOn = enabled;

  if (enabled) {
    Serial.println(F("Light turned ON."));
  } else {
    Serial.println(F("Light turned OFF."));
  }
}
