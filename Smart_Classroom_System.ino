#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>
#include <DHT.h>
#include <IRremote.hpp>

// -----------------------------
// Pin mapping - update these to match your wiring
// -----------------------------
const uint8_t RFID_SS_PIN = 10;
const uint8_t RFID_RST_PIN = 8;
const uint8_t PIR_PIN = 2;
const uint8_t IR_RECEIVER_PIN = 7;
const uint8_t DHT_PIN = A0;
const uint8_t LDR_PIN = A1;
const uint8_t LIGHT_PIN = A2;
const uint8_t FAN_PIN = 4;
const uint8_t DOOR_SERVO_PIN = 6;
const uint8_t BLIND_SERVO_PIN = 5;
const uint8_t SCREEN_SERVO_PIN = 3;

// Output logic levels - change if you use an active-low relay module
const uint8_t LIGHT_ON_LEVEL = HIGH;
const uint8_t LIGHT_OFF_LEVEL = LOW;
const uint8_t FAN_ON_LEVEL = HIGH;
const uint8_t FAN_OFF_LEVEL = LOW;

// Sensor/actuator behaviour
const uint8_t PIR_ACTIVE_LEVEL = HIGH;
const unsigned long ROOM_EMPTY_TIMEOUT_MS = 5UL * 60UL * 1000UL;
const unsigned long DOOR_UNLOCK_TIME_MS = 3000UL;
const unsigned long DHT_READ_INTERVAL_MS = 2000UL;
const unsigned long LDR_READ_INTERVAL_MS = 700UL;
const unsigned long STATUS_PRINT_INTERVAL_MS = 5000UL;

const float FAN_ON_TEMPERATURE = 28.0;
const float FAN_OFF_TEMPERATURE = 26.0;

const int LDR_BRIGHT_THRESHOLD = 750;
const int LDR_DARK_THRESHOLD = 450;

// Servo angles - tune these to your mechanical system
const uint8_t DOOR_LOCKED_ANGLE = 10;
const uint8_t DOOR_UNLOCKED_ANGLE = 95;
const uint8_t BLIND_OPEN_ANGLE = 15;
const uint8_t BLIND_HALF_ANGLE = 90;
const uint8_t BLIND_CLOSED_ANGLE = 165;
const uint8_t SCREEN_UP_ANGLE = 20;
const uint8_t SCREEN_DOWN_ANGLE = 150;

// Replace these with the raw IR codes printed on Serial Monitor
const IRRawDataType IR_CODE_SCREEN_UP = 0xE718FF00;
const IRRawDataType IR_CODE_SCREEN_DOWN = 0xAD52FF00;

#define DHT_TYPE DHT11

struct AuthorizedCard {
  byte uid[10];
  byte size;
};

// Authorized cards learned from your test results
const AuthorizedCard AUTHORIZED_CARDS[] = {
  {{0x15, 0x47, 0x1F, 0x07}, 4},  // Blue keychain
  {{0xEB, 0x18, 0xF0, 0x06}, 4},  // White card
};

MFRC522 rfid(RFID_SS_PIN, RFID_RST_PIN);
DHT dht(DHT_PIN, DHT_TYPE);
Servo doorServo;
Servo blindServo;
Servo screenServo;

bool roomOccupied = false;
bool lightsOn = false;
bool fanOn = false;
bool doorUnlocked = false;

uint8_t currentBlindAngle = 255;
uint8_t currentScreenAngle = 255;

unsigned long lastMotionMillis = 0;
unsigned long doorUnlockMillis = 0;
unsigned long lastDhtReadMillis = 0;
unsigned long lastLdrReadMillis = 0;
unsigned long lastStatusPrintMillis = 0;

float lastTemperature = NAN;
int lastLdrValue = 0;

bool isAuthorizedCard(const MFRC522::Uid &uid);
void printUid(const MFRC522::Uid &uid);
void setLights(bool enabled);
void setFan(bool enabled);
void lockDoor();
void unlockDoor(unsigned long now);
void setBlindAngle(uint8_t angle);
void setScreenAngle(uint8_t angle);
void handleRfid(unsigned long now);
void handlePir(unsigned long now);
void handleDoorTimer(unsigned long now);
void handleClimate(unsigned long now);
void handleBlinds(unsigned long now);
void handleIrRemote();
void handleRoomEmpty(unsigned long now);
void printStatus(unsigned long now);

void setup() {
  Serial.begin(115200);

  pinMode(PIR_PIN, INPUT);
  pinMode(LIGHT_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);

  setLights(false);
  setFan(false);

  doorServo.attach(DOOR_SERVO_PIN);
  blindServo.attach(BLIND_SERVO_PIN);
  screenServo.attach(SCREEN_SERVO_PIN);

  lockDoor();
  setBlindAngle(BLIND_OPEN_ANGLE);
  setScreenAngle(SCREEN_UP_ANGLE);

  SPI.begin();
  rfid.PCD_Init();
  dht.begin();
  IrReceiver.begin(IR_RECEIVER_PIN, ENABLE_LED_FEEDBACK);

  Serial.println(F("Smart Classroom System is ready."));
  Serial.println(F("Scan a card or press IR remote buttons to test."));
}

void loop() {
  const unsigned long now = millis();

  handleRfid(now);
  handlePir(now);
  handleDoorTimer(now);
  handleClimate(now);
  handleBlinds(now);
  handleIrRemote();
  handleRoomEmpty(now);
  printStatus(now);
}

bool isAuthorizedCard(const MFRC522::Uid &uid) {
  for (size_t i = 0; i < sizeof(AUTHORIZED_CARDS) / sizeof(AUTHORIZED_CARDS[0]); ++i) {
    if (uid.size != AUTHORIZED_CARDS[i].size) {
      continue;
    }

    bool match = true;
    for (byte j = 0; j < uid.size; ++j) {
      if (uid.uidByte[j] != AUTHORIZED_CARDS[i].uid[j]) {
        match = false;
        break;
      }
    }

    if (match) {
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

void setLights(bool enabled) {
  digitalWrite(LIGHT_PIN, enabled ? LIGHT_ON_LEVEL : LIGHT_OFF_LEVEL);
  lightsOn = enabled;
}

void setFan(bool enabled) {
  digitalWrite(FAN_PIN, enabled ? FAN_ON_LEVEL : FAN_OFF_LEVEL);
  fanOn = enabled;
}

void lockDoor() {
  doorServo.write(DOOR_LOCKED_ANGLE);
  doorUnlocked = false;
}

void unlockDoor(unsigned long now) {
  doorServo.write(DOOR_UNLOCKED_ANGLE);
  doorUnlocked = true;
  doorUnlockMillis = now;
}

void setBlindAngle(uint8_t angle) {
  if (currentBlindAngle == angle) {
    return;
  }

  blindServo.write(angle);
  currentBlindAngle = angle;
}

void setScreenAngle(uint8_t angle) {
  if (currentScreenAngle == angle) {
    return;
  }

  screenServo.write(angle);
  currentScreenAngle = angle;
}

void handleRfid(unsigned long now) {
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) {
    return;
  }

  if (isAuthorizedCard(rfid.uid)) {
    Serial.print(F("Authorized card detected: "));
    printUid(rfid.uid);
    Serial.println();

    roomOccupied = true;
    lastMotionMillis = now;
    setLights(true);
    unlockDoor(now);
  } else {
    Serial.print(F("Unauthorized card rejected: "));
    printUid(rfid.uid);
    Serial.println();
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}

void handlePir(unsigned long now) {
  if (!roomOccupied) {
    return;
  }

  if (digitalRead(PIR_PIN) == PIR_ACTIVE_LEVEL) {
    lastMotionMillis = now;
  }
}

void handleDoorTimer(unsigned long now) {
  if (!doorUnlocked) {
    return;
  }

  if (now - doorUnlockMillis >= DOOR_UNLOCK_TIME_MS) {
    lockDoor();
    Serial.println(F("Door locked again."));
  }
}

void handleClimate(unsigned long now) {
  if (now - lastDhtReadMillis < DHT_READ_INTERVAL_MS) {
    return;
  }

  lastDhtReadMillis = now;
  const float temperature = dht.readTemperature();

  if (isnan(temperature)) {
    Serial.println(F("DHT11 read failed."));
    return;
  }

  lastTemperature = temperature;

  if (!roomOccupied) {
    setFan(false);
    return;
  }

  if (!fanOn && temperature >= FAN_ON_TEMPERATURE) {
    setFan(true);
    Serial.println(F("Fan turned ON."));
  } else if (fanOn && temperature <= FAN_OFF_TEMPERATURE) {
    setFan(false);
    Serial.println(F("Fan turned OFF."));
  }
}

void handleBlinds(unsigned long now) {
  if (now - lastLdrReadMillis < LDR_READ_INTERVAL_MS) {
    return;
  }

  lastLdrReadMillis = now;
  lastLdrValue = analogRead(LDR_PIN);

  if (lastLdrValue >= LDR_BRIGHT_THRESHOLD) {
    setBlindAngle(BLIND_CLOSED_ANGLE);
  } else if (lastLdrValue <= LDR_DARK_THRESHOLD) {
    setBlindAngle(BLIND_OPEN_ANGLE);
  } else {
    setBlindAngle(BLIND_HALF_ANGLE);
  }
}

void handleIrRemote() {
  if (!IrReceiver.decode()) {
    return;
  }

  const IRRawDataType rawCode = IrReceiver.decodedIRData.decodedRawData;

  Serial.print(F("IR raw code: 0x"));
  Serial.println((uint32_t)rawCode, HEX);

  if (rawCode == IR_CODE_SCREEN_UP) {
    setScreenAngle(SCREEN_UP_ANGLE);
    Serial.println(F("Projection screen moved UP."));
  } else if (rawCode == IR_CODE_SCREEN_DOWN) {
    setScreenAngle(SCREEN_DOWN_ANGLE);
    Serial.println(F("Projection screen moved DOWN."));
  }

  IrReceiver.resume();
}

void handleRoomEmpty(unsigned long now) {
  if (!roomOccupied) {
    return;
  }

  if (now - lastMotionMillis >= ROOM_EMPTY_TIMEOUT_MS) {
    roomOccupied = false;
    setLights(false);
    setFan(false);
    Serial.println(F("No motion for 5 minutes. Room set to empty."));
  }
}

void printStatus(unsigned long now) {
  if (now - lastStatusPrintMillis < STATUS_PRINT_INTERVAL_MS) {
    return;
  }

  lastStatusPrintMillis = now;

  Serial.print(F("occupied="));
  Serial.print(roomOccupied ? F("YES") : F("NO"));
  Serial.print(F(" | lights="));
  Serial.print(lightsOn ? F("ON") : F("OFF"));
  Serial.print(F(" | fan="));
  Serial.print(fanOn ? F("ON") : F("OFF"));
  Serial.print(F(" | ldr="));
  Serial.print(lastLdrValue);
  Serial.print(F(" | temp="));

  if (isnan(lastTemperature)) {
    Serial.println(F("N/A"));
  } else {
    Serial.println(lastTemperature);
  }
}
