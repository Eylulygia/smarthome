#include <SPI.h>
#include <MFRC522.h>

// Arduino Uno + RC522
// SDA(SS) -> D10
// SCK     -> D13
// MOSI    -> D11
// MISO    -> D12
// RST     -> D8
// 3.3V    -> 3.3V
// GND     -> GND

const byte RFID_SS_PIN = 10;
const byte RFID_RST_PIN = 8;

struct AuthorizedCard {
  byte uid[10];
  byte size;
};

// Authorized cards learned from your test results.
const AuthorizedCard AUTHORIZED_CARDS[] = {
  {{0x15, 0x47, 0x1F, 0x07}, 4},  // Blue keychain
  {{0xEB, 0x18, 0xF0, 0x06}, 4},  // White card
};

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);

bool isAuthorizedCard(const MFRC522::Uid &uid);
void printUid(const MFRC522::Uid &uid);
void printReaderStatus();

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  delay(5);
  rfid.PCD_AntennaOn();
  rfid.PCD_SetAntennaGain(MFRC522::RxGain_max);

  Serial.println(F("---- RFID READER READY ----"));
  Serial.println(F("Show your card to the reader."));
  Serial.println(F("Antenna gain set to MAX."));
  printReaderStatus();
}

void loop() {
  if (!rfid.PICC_IsNewCardPresent()) {
    return;
  }

  if (!rfid.PICC_ReadCardSerial()) {
    return;
  }

  Serial.print(F("Kart bulundu. UID: "));
  printUid(rfid.uid);

  if (isAuthorizedCard(rfid.uid)) {
    Serial.println(F(" -> Yetkili kart"));
  } else {
    Serial.println(F(" -> Yetkisiz kart"));
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
  delay(1000);
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

void printReaderStatus() {
  byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);

  Serial.print(F("Reader firmware register: 0x"));
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF) {
    Serial.println(F("SPI communication problem. Check wiring and power."));
    return;
  }

  if (version == 0x88) {
    Serial.println(F("Clone/Fudan chip detected."));
  } else if (version == 0x90 || version == 0x91 || version == 0x92) {
    Serial.println(F("MFRC522-compatible reader detected."));
  } else {
    Serial.println(F("Unknown version. Reader may still work, but wiring or module quality may be an issue."));
  }
}
