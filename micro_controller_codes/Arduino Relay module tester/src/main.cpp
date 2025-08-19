#include <Arduino.h>

const int buttonPin = 2;
const int relayPin = 5;
bool relayState = true;
bool ledState = false;

void onButtonFall() {
  relayState = !relayState;
  ledState = !ledState;
  digitalWrite(relayPin, relayState ? HIGH : LOW);
}

void setup() {
  relayState = true;
  ledState = false;

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, HIGH);
  Serial.begin(9600);
  Serial.println("Relay High");
  Serial.println("LED Off");

  attachInterrupt(digitalPinToInterrupt(buttonPin), onButtonFall, FALLING);
}

void loop() {
  Serial.print("Relay ");
  Serial.println(relayState ? "HIGH" : "LOW");
  Serial.print("LED ");
  Serial.println(ledState ? "On" : "Off");
  delay(2000);
}
