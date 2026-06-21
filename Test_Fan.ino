const byte FAN_IN1_PIN = 7;
const byte FAN_IN2_PIN = A1;

void setup() {
  Serial.begin(115200);
  
  pinMode(FAN_IN1_PIN, OUTPUT);
  pinMode(FAN_IN2_PIN, OUTPUT);

  Serial.println("Fan calistiriliyor...");
  
  // Fana calis komutu gonder
  digitalWrite(FAN_IN1_PIN, HIGH);
  digitalWrite(FAN_IN2_PIN, LOW);
}

void loop() {
  // Sadece fanin donmesini bekliyoruz, ekstra bir kod yok.
}
