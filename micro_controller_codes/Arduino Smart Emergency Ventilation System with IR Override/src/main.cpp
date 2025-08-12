#include <IRremote.h>

const int flamePin = 2;
const int gasPin = A0;
const int relayPin = 4;
const int irPin = 3;
const int ledPin = 5;

void setup() {
  Serial.begin(9600);
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK);
}
void loop() {
  int gasValue = analogRead(gasPin);
  bool flameDetected = digitalRead(flamePin) == HIGH;
  bool danger = flameDetected || gasValue > 400;

  // Debug info
  Serial.print("Gas: "); Serial.print(gasValue);
  Serial.print(" | Flame: "); Serial.print(flameDetected ? "YES" : "NO");
  Serial.print(" | Danger: "); Serial.println(danger ? "YES" : "NO");

  digitalWrite(ledPin, danger ? HIGH : LOW);
  Serial.print("LED is: ");
  Serial.println(danger ? "Turn on" : "Turn off");

  if (danger) {
    Serial.println("Danger detected! Relay set to LOW.");
    digitalWrite(relayPin, HIGH);
  }
  
  if (IrReceiver.decode()) {
    uint32_t value = IrReceiver.decodedIRData.decodedRawData; // LSB-first in 3.x
    Serial.print("IR Code received: 0x");
    Serial.println(value, HEX);

    if (value == 0xE9167B80) {
      Serial.println("Manually triggered.");
      digitalWrite(relayPin, HIGH);
    } else if (value == 0xE8177B80) {
      if (!danger) {
        Serial.println("Reset system.");
        digitalWrite(relayPin, LOW);
      } else {
        Serial.println("Reset blocked: Danger still present.");
      }
    }
    IrReceiver.resume();
  }

  delay(500); // Slow down output for readability
}
