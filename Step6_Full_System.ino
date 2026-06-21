#include <IRremote.hpp>
#include <MFRC522.h>
#include <SPI.h>
#include <Servo.h>

// =============================================
// AKILLI SINIF - TUM SENSOR ENTEGRASYONU
// =============================================
//
// RC522 RFID
// SDA(SS) -> D10
// SCK     -> D13
// MOSI    -> D11
// MISO    -> D12
// RST     -> D9
// 3.3V    -> 3.3V
// GND     -> GND
//
// Door Servo
// Signal  -> D6
//
// PIR (HC-SR505)
// OUT     -> A2
// VCC     -> 5V
// GND     -> GND
//
// Light output (RGB LED - Mavi renk)
// Mavi bacak -> D7 (220 ohm direnc ile)
// Ortak Cathode (en uzun bacak) -> GND
//
// IR Receiver
// OUT/S   -> A5 (Yesil Kablo)
// VCC     -> 5V (Mavi Kablo)
// GND     -> GND (Mor Kablo)
//
// Screen Servo (Projeksiyon Perdesi)
// Signal  -> D3
//
// LDR (Isik Sensoru) - YENI
// Sinyal  -> A0 (10K direnc ile voltage divider)
// VCC     -> 5V
// GND     -> GND (direnc uzerinden)
// =============================================

const byte RFID_SS_PIN = 10;
const byte RFID_RST_PIN = 9;
const byte DOOR_SERVO_PIN = 6;
const byte PIR_PIN = A2;        // PIR artik A2'de
const byte LIGHT_PIN = 7;       // Mavi LED D7'de
const byte IR_RECEIVER_PIN = A5;
const byte SCREEN_SERVO_PIN = 3;
const byte LDR_PIN = A0;        // YENI: LDR sensoru

// Kapi ve Sure Ayarlari
const byte DOOR_LOCKED_ANGLE = 0;
const byte DOOR_UNLOCKED_ANGLE = 90;
const unsigned long DOOR_OPEN_TIME_MS = 7000;
const unsigned long PIR_WARMUP_TIME_MS = 30000;
const unsigned long ROOM_EMPTY_TIMEOUT_MS = 15000;

// Projeksiyon Perdesi Aci Ayarlari
const byte SCREEN_UP_ANGLE = 20;
const byte SCREEN_DOWN_ANGLE = 150;

// IR Kumanda Kodlari
const IRRawDataType IR_CODE_SCREEN_UP = 0xE718FF00;
const IRRawDataType IR_CODE_SCREEN_DOWN = 0xAD52FF00;

// LDR Esik Degerleri (YENI)
const int LDR_DARK_THRESHOLD = 300;   // Bu degerin altiysa karanlik
const int LDR_BRIGHT_THRESHOLD = 700; // Bu degerin ustuyse cok aydinlik
const unsigned long LDR_READ_INTERVAL_MS = 2000; // 2 saniyede bir olcum

const byte LIGHT_ON_LEVEL = HIGH;
const byte LIGHT_OFF_LEVEL = LOW;
const byte PIR_ACTIVE_LEVEL = HIGH;

struct AuthorizedCard {
  byte uid[10];
  byte size;
};

const AuthorizedCard AUTHORIZED_CARDS[] = {
    {{0x15, 0x47, 0x1F, 0x07}, 4}, // Blue keychain
    {{0xEB, 0x18, 0xF0, 0x06}, 4}, // White card
};

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo doorServo;
Servo screenServo;

bool roomOccupied = false;
bool doorIsOpen = false;
bool lightIsOn = false;
bool pirWasActive = false;

byte currentScreenAngle = 255;
unsigned long doorOpenAt = 0;
unsigned long lastMotionAt = 0;
unsigned long pirWarmupStartedAt = 0;
unsigned long lastLdrReadAt = 0; // YENI

// Fonksiyon bildirimleri
bool isAuthorizedCard(const MFRC522::Uid &uid);
void printUid(const MFRC522::Uid &uid);
void openDoor(unsigned long now);
void closeDoor();
void setLight(bool enabled);
void setScreenAngle(byte angle);
void handleIrRemote();
void handleLDR(unsigned long now); // YENI

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  // LDR analog okuma, pinMode gerekmez

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

  setLight(false);
  pirWarmupStartedAt = millis();

  Serial.println(F("=================================="));
  Serial.println(F("AKILLI SINIF SISTEMI HAZIR"));
  Serial.println(F("=================================="));
  Serial.println(F("RFID + Kapi + PIR + Isik + IR Kumanda"));
  Serial.println(F("+ Projeksiyon + LDR Isik Sensoru"));
}

void loop() {
  unsigned long now = millis();
  bool pirActive = digitalRead(PIR_PIN) == PIR_ACTIVE_LEVEL;

  // IR kumanda sinyallerini kontrol et
  handleIrRemote();

  // LDR isik seviyesini oku ve raporla
  handleLDR(now);

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
    Serial.println(F("No motion detected. Room marked empty."));
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

// YENI FONKSIYON: LDR isik seviyesini okur ve raporlar
void handleLDR(unsigned long now) {
  if (now - lastLdrReadAt < LDR_READ_INTERVAL_MS) {
    return;
  }
  lastLdrReadAt = now;

  int ldrValue = analogRead(LDR_PIN);

  Serial.print(F("LDR: "));
  Serial.print(ldrValue);

  if (ldrValue < LDR_DARK_THRESHOLD) {
    Serial.println(F(" (Karanlik)"));
  } else if (ldrValue < LDR_BRIGHT_THRESHOLD) {
    Serial.println(F(" (Normal Isik)"));
  } else {
    Serial.println(F(" (Cok Aydinlik)"));
  }
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
