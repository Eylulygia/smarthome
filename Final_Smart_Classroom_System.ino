#include <IRremote.hpp>
#include <MFRC522.h>
#include <SPI.h>
#include <Servo.h>

// =============================================
// AKILLI SINIF OTOMASYONU - FINAL SURUM (STEP 7)
// =============================================
// Donanim Baglantilari:
// RFID: D10, D13, D11, D12, D9
// Kapi Servo: D6
// PIR Hareket: A2
// Mavi LED: D7 (En uzun bacak GND'de)
// IR Kumanda: A5
// Projeksiyon Servo: D3
// LDR Isik: A0
// Pencere Perdesi Servo: D5
// =============================================

const byte RFID_SS_PIN = 10;
const byte RFID_RST_PIN = 9;
const byte DOOR_SERVO_PIN = 6;
const byte PIR_PIN = A2;
const byte LIGHT_PIN = 7;
const byte IR_RECEIVER_PIN = A5;
const byte SCREEN_SERVO_PIN = 3;
const byte LDR_PIN = A0;
const byte BLIND_SERVO_PIN = 5;

// Zamanlama ve Aci Ayarlari
const unsigned long DOOR_OPEN_TIME_MS = 7000;
const unsigned long ROOM_EMPTY_TIMEOUT_MS = 30000;
const byte DOOR_LOCKED_ANGLE = 90;
const byte DOOR_UNLOCKED_ANGLE = 0;
const byte SCREEN_UP_ANGLE = 20;
const byte SCREEN_DOWN_ANGLE = 150;
const byte BLIND_OPEN_ANGLE = 15;
const byte BLIND_CLOSED_ANGLE = 165;

// LDR Sensör Esik Degerleri
const int LDR_BRIGHT_THRESHOLD = 600;
const int LDR_DARK_THRESHOLD = 350;
const int LDR_HYSTERESIS = 30;

// IR Kumanda Kodlari (Protokol: NEC)
const IRRawDataType IR_CODE_SCREEN_UP = 0xE718FF00;
const IRRawDataType IR_CODE_SCREEN_DOWN = 0xAD52FF00;

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo doorServo, screenServo, blindServo;

bool roomOccupied = false;
bool doorIsOpen = false;
bool pirWasActive = false;
byte currentBlindAngle = 255;
unsigned long doorOpenAt = 0;
unsigned long lastMotionAt = 0;
unsigned long lastLdrReadAt = 0;

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, LOW); // Baslangicta isik kapali

  SPI.begin();
  rfid.PCD_Init();
  
  doorServo.attach(DOOR_SERVO_PIN);
  doorServo.write(DOOR_LOCKED_ANGLE);
  
  screenServo.attach(SCREEN_SERVO_PIN);
  screenServo.write(SCREEN_UP_ANGLE);
  
  blindServo.attach(BLIND_SERVO_PIN);
  blindServo.write(90); // Perdeyi orta konumda baslat

  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK);

  Serial.println(F("======================================="));
  Serial.println(F("   AKILLI SINIF SISTEMI BASLATILDI     "));
  Serial.println(F("======================================="));
}

void loop() {
  unsigned long now = millis();
  bool pirActive = (digitalRead(PIR_PIN) == HIGH);

  handleIrRemote();
  handleBlinds(now);

  // Kapi Kilidi Zamanlamasi
  if (doorIsOpen && now - doorOpenAt >= DOOR_OPEN_TIME_MS) {
    doorServo.write(DOOR_LOCKED_ANGLE);
    doorIsOpen = false;
    Serial.println(F("Kapı otomatik kilitlendi."));
  }

  // Hareket Takibi ve Akilli Isik Kontrolu
  if (pirActive) {
    if (!pirWasActive) {
      Serial.println(F("Hareket algılandı."));
      if (!roomOccupied) {
        roomOccupied = true;
        digitalWrite(LIGHT_PIN, HIGH);
        Serial.println(F("Sistem: Oda tekrar aktif hale getirildi!"));
      }
    }
    lastMotionAt = now;
  }
  pirWasActive = pirActive;

  // Oda Bosalma Kontrolu
  if (roomOccupied && now - lastMotionAt >= ROOM_EMPTY_TIMEOUT_MS) {
    roomOccupied = false;
    digitalWrite(LIGHT_PIN, LOW);
    Serial.println(F("Oda bos: Isiklar kapatildi."));
  }

  // RFID Kart Okuma
  if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) {
    if (isAuthorizedCard(rfid.uid)) {
      Serial.println(F("Yetkili Kart: Giris Onaylandi."));
      doorServo.write(DOOR_UNLOCKED_ANGLE);
      doorIsOpen = true;
      doorOpenAt = now;
      roomOccupied = true;
      lastMotionAt = now;
      digitalWrite(LIGHT_PIN, HIGH);
    }
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
  }
}

void handleBlinds(unsigned long now) {
  if (now - lastLdrReadAt < 2000) return;
  lastLdrReadAt = now;

  int ldrValue = analogRead(LDR_PIN);
  
  if (ldrValue > LDR_BRIGHT_THRESHOLD + LDR_HYSTERESIS) {
    if (currentBlindAngle != BLIND_CLOSED_ANGLE) {
      blindServo.write(BLIND_CLOSED_ANGLE);
      currentBlindAngle = BLIND_CLOSED_ANGLE;
      Serial.println(F("Sensor: Gunesli -> Perde Kapandi."));
    }
  } else if (ldrValue < LDR_DARK_THRESHOLD - LDR_HYSTERESIS) {
    if (currentBlindAngle != BLIND_OPEN_ANGLE) {
      blindServo.write(BLIND_OPEN_ANGLE);
      currentBlindAngle = BLIND_OPEN_ANGLE;
      Serial.println(F("Sensor: Karanlik -> Perde Acildi."));
    }
  }
}

void handleIrRemote() {
  if (IrReceiver.decode()) {
    const IRRawDataType code = IrReceiver.decodedIRData.decodedRawData;
    if (code == IR_CODE_SCREEN_UP) {
      screenServo.write(SCREEN_UP_ANGLE);
      Serial.println(F("Kumanda: Projeksiyon Perdesi YUKARI."));
    } else if (code == IR_CODE_SCREEN_DOWN) {
      screenServo.write(SCREEN_DOWN_ANGLE);
      Serial.println(F("Kumanda: Projeksiyon Perdesi ASAGI."));
    }
    IrReceiver.resume();
  }
}

bool isAuthorizedCard(const MFRC522::Uid &uid) {
  const byte auth1[] = {0x15, 0x47, 0x1F, 0x07};
  const byte auth2[] = {0xEB, 0x18, 0xF0, 0x06};
  
  bool match1 = true, match2 = true;
  for (byte i = 0; i < 4; i++) {
    if (uid.uidByte[i] != auth1[i]) match1 = false;
    if (uid.uidByte[i] != auth2[i]) match2 = false;
  }
  return (match1 || match2);
}
