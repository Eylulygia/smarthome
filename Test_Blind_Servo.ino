#include <Servo.h>

// Sadece D5 pinindeki perde servosunu test eder.
// Diger sensorler ve kumandalar bu testte devre disidir.

const byte TEST_SERVO_PIN = 5; 
Servo testServo;

void setup() {
  Serial.begin(115200);
  Serial.println("--- SERVO D5 TESTI BASLADI ---");
  
  testServo.attach(TEST_SERVO_PIN);
}

void loop() {
  Serial.println("Derece: 15");
  testServo.write(15);
  delay(2000);
  
  Serial.println("Derece: 90");
  testServo.write(90);
  delay(2000);
  
  Serial.println("Derece: 165");
  testServo.write(165);
  delay(2000);
}
