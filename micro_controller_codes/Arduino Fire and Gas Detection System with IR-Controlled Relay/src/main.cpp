#include <IRremote.h>

const uint8_t flamePin = 2;
const uint8_t gasPin = A0;
const uint8_t relayPin = 4;
const uint8_t irPin = 3;


void setup() {
  Serial.begin(9600);
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);

  IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK);
}

void loop() {
  int gasValue = analogRead(gasPin);
  bool flameDetected = digitalRead(flamePin) == HIGH;


  if (flameDetected || gasValue >= 400) {
    digitalWrite(relayPin, HIGH);
    Serial.println("Hazard detected! Alarm activated.");
  }

  if (IrReceiver.decode()) {
    uint32_t value = IrReceiver.decodedIRData.decodedRawData;

    Serial.print("IR code: ");
    Serial.println(value, HEX);
    if (value == 0xE9167B80){
      digitalWrite(relayPin, HIGH);
      Serial.println("Manual Alarm triggered.");
    } else if (value == 0xE8177B80) {
      digitalWrite(relayPin, LOW);
      Serial.println("System Reset.");
    }
    IrReceiver.resume();
  }

  Serial.print("Flame detected: ");
  Serial.println(flameDetected);

  Serial.print("Gas Value: ");
  Serial.println(gasValue);

}