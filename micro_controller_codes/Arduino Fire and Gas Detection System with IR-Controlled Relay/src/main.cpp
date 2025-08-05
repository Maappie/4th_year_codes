#include <Arduino.h>
#include <IRremote.hpp>

const int flamePin = 7;
const int gasPin = A0;
const int relayPin = 4;
const int irPin = 2;  

void setup() {
  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);
  Serial.begin(9600);

  IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK);
}

void loop() {
  int gasValue = analogRead(gasPin);
  bool flameDetected = digitalRead(flamePin) == LOW;

  if (flameDetected || gasValue > 400) {
    digitalWrite(relayPin, HIGH);
  } else {
    digitalWrite(relayPin, LOW);
  }

  if (IrReceiver.decode()) {
    uint32_t irCode = IrReceiver.decodedIRData.decodedRawData;

    if (irCode == 0xFF30CF) {
      digitalWrite(relayPin, HIGH);
    } else if (irCode == 0xFF18E7) {
      digitalWrite(relayPin, LOW);
    }

    IrReceiver.resume(); 
  }
}
