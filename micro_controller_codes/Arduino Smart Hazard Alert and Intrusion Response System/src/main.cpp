#include <IRremote.h>

const uint8_t flamePin = 2;
const uint8_t gasPin = A0;
const uint8_t motionPin = 6;
const uint8_t relayPin = 4;
const uint8_t irPin = 3;

bool hazardDetected = false;

void setup() {
  Serial.begin(9600);
  pinMode(flamePin, INPUT);
  pinMode(motionPin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK);
}

void loop() {
  int gasValue = analogRead(gasPin);
  bool flameDetected = digitalRead(flamePin) == HIGH;
  bool motionDetected = digitalRead(motionPin) == LOW;

  hazardDetected = flameDetected || motionDetected || gasValue >= 400;

  if (hazardDetected) {
    digitalWrite(relayPin, HIGH);
    Serial.println("Hazard detected! Alarm activated.");
  }

  if (IrReceiver.decode()) {
    uint32_t value = IrReceiver.decodedIRData.decodedRawData; // LSB-first in 3.x

    Serial.print("IR code: ");
    Serial.println(value, HEX);
    if (value == 0xE9167B80){
      digitalWrite(relayPin, HIGH);
      Serial.println("Manual Alarm triggered.");
    } else if (value == 0xE8177B80) {
      if (!hazardDetected){
        digitalWrite(relayPin, LOW);
        Serial.println("System Reset.");
      } else {
        Serial.println("Cannot reset during hazard.");
      }
    }
    IrReceiver.resume();
  }

  Serial.print("Flame detected: ");
  Serial.println(flameDetected);

  Serial.print("Motion detected: ");
  Serial.println(motionDetected);

  Serial.print("Gas Value: ");
  Serial.println(gasValue);

}