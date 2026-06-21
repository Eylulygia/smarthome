// =============================================
// PIR (HC-SR505) ve LDR Test Kodu
// =============================================
// PIR Sinyal  -> A2
// PIR VCC     -> 5V
// PIR GND     -> GND
//
// LDR Sinyal  -> A0 (10K direnc ile voltage divider)
// LDR VCC     -> 5V
// LDR GND     -> GND (direnc uzerinden)
// =============================================

const byte PIR_PIN = A2;
const byte LDR_PIN = A0;

void setup() {
  Serial.begin(115200);
  pinMode(PIR_PIN, INPUT);
  // LDR analog okuma, pinMode gerekmez

  Serial.println(F("=================================="));
  Serial.println(F("PIR + LDR Sensor Test Basladi"));
  Serial.println(F("=================================="));
  Serial.println(F("PIR: Hareket varsa HIGH (1), yoksa LOW (0)"));
  Serial.println(F("LDR: 0=Karanlik, 1023=Cok Aydinlik"));
  Serial.println();
}

void loop() {
  // PIR okuma (dijital: HIGH veya LOW)
  int pirValue = digitalRead(PIR_PIN);

  // LDR okuma (analog: 0 - 1023)
  int ldrValue = analogRead(LDR_PIN);

  Serial.print(F("PIR: "));
  if (pirValue == HIGH) {
    Serial.print(F("HAREKET VAR!  "));
  } else {
    Serial.print(F("Hareket yok   "));
  }

  Serial.print(F("| LDR: "));
  Serial.print(ldrValue);

  if (ldrValue < 300) {
    Serial.println(F("  (Karanlik)"));
  } else if (ldrValue < 700) {
    Serial.println(F("  (Normal Isik)"));
  } else {
    Serial.println(F("  (Cok Aydinlik)"));
  }

  delay(500); // Yarim saniyede bir okuma
}
