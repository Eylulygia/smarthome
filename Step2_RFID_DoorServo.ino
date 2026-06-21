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
// Servo
// Signal  -> D6
// VCC     -> 5V
// GND     -> GND
//
// If the servo shakes or resets the Arduino, use an external 5V supply.
// In that case, keep Arduino GND and external supply GND connected together.

const byte RFID_SS_PIN = 10;
const byte RFID_RST_PIN = 8;
const byte DOOR_SERVO_PIN = 6;

const byte DOOR_LOCKED_ANGLE = 0;
const byte DOOR_UNLOCKED_ANGLE = 90;
const unsigned long DOOR_OPEN_TIME_MS = 7000;

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

bool doorIsOpen = false;
unsigned long doorOpenAt = 0;

bool isAuthorizedCard(const MFRC522::Uid &uid);
void printUid(const MFRC522::Uid &uid);
void openDoor(unsigned long now);
void closeDoor();

void setup() {
  Serial.begin(115200);

  SPI.begin();
  rfid.PCD_Init();
  delay(5);
  rfid.PCD_AntennaOn();
  rfid.PCD_SetAntennaGain(MFRC522::RxGain_max);

  doorServo.attach(DOOR_SERVO_PIN);
  closeDoor();

  Serial.println(F("---- RFID + DOOR SERVO READY ----"));
  Serial.println(F("Show an authorized card to unlock the door."));
}

void loop() {
  unsigned long now = millis();

  if (doorIsOpen && now - doorOpenAt >= DOOR_OPEN_TIME_MS) {
    closeDoor();
    Serial.println(F("Door locked again."));
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
