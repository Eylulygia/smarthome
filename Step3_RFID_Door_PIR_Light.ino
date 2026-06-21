#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// RC522
// SDA(SS) -> D10
// SCK     -> D13
// MOSI    -> D11
// MISO    -> D12
// RST     -> D8
// 3.3V    -> 3.3V
// GND     -> GND
//
// Door servo
// Signal  -> D6
// VCC     -> 5V
// GND     -> GND
//
// PIR
// OUT     -> D2
// VCC     -> 5V
// GND     -> GND
//
// Light output
// LED anode / relay IN -> A2
// LED cathode / relay GND -> GND

const byte RFID_SS_PIN = 10;
const byte RFID_RST_PIN = 8;
const byte DOOR_SERVO_PIN = 6;
const byte PIR_PIN = 2;
const byte LIGHT_PIN = A2;

const byte DOOR_LOCKED_ANGLE = 0;
const byte DOOR_UNLOCKED_ANGLE = 90;
const unsigned long DOOR_OPEN_TIME_MS = 7000;
const unsigned long PIR_WARMUP_TIME_MS = 30000;

// Use 15000 for easy testing. Change to 300000 for 5 minutes in the final system.
const unsigned long ROOM_EMPTY_TIMEOUT_MS = 15000;

const byte LIGHT_ON_LEVEL = HIGH;
const byte LIGHT_OFF_LEVEL = LOW;
const byte PIR_ACTIVE_LEVEL = HIGH;

struct AuthorizedCard {
  byte uid[10];
  byte size;
};

const AuthorizedCard AUTHORIZED_CARDS[] = {
  {{0x15, 0x47, 0x1F, 0x07}, 4},  // Blue keychain
  {{0xEB, 0x18, 0xF0, 0x06}, 4},  // White card
};

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
Servo doorServo;

bool roomOccupied = false;
bool doorIsOpen = false;
bool lightIsOn = false;
bool pirWasActive = false;

unsigned long doorOpenAt = 0;
unsigned long lastMotionAt = 0;
unsigned long pirWarmupStartedAt = 0;

bool isAuthorizedCard(const MFRC522::Uid &uid);
void printUid(const MFRC522::Uid &uid);
void openDoor(unsigned long now);
void closeDoor();
void setLight(bool enabled);

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LIGHT_PIN, OUTPUT);

  SPI.begin();
  rfid.PCD_Init();
  delay(5);
  rfid.PCD_AntennaOn();
  rfid.PCD_SetAntennaGain(MFRC522::RxGain_max);

  doorServo.attach(DOOR_SERVO_PIN);
  closeDoor();
  setLight(false);
  pirWarmupStartedAt = millis();

  Serial.println(F("---- STEP 3 READY ----"));
  Serial.println(F("Authorized card opens the door and turns the light on."));
  Serial.println(F("If PIR sees no motion, the light turns off after timeout."));
  Serial.println(F("PIR warmup: wait about 30 seconds after power-on."));
}

void loop() {
  unsigned long now = millis();
  bool pirActive = digitalRead(PIR_PIN) == PIR_ACTIVE_LEVEL;

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

  if (roomOccupied && now - pirWarmupStartedAt >= PIR_WARMUP_TIME_MS && pirActive) {
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

bool isAuthorizedCard(const MFRC522::Uid &uid) {
  for (size_t i = 0; i < sizeof(AUTHORIZED_CARDS) / sizeof(AUTHORIZED_CARDS[0]); ++i) {
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
