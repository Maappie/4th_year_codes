#include <IRremote.h>
const int flamePin = 2;
const int gasPin = A0;
const int motionPin = 6;
const int relayPin = 4;
const int irPin = 3;

bool manualOverride = false;
bool overrideState = false;

void setup(){
  Serial.begin(9600);

  pinMode(flamePin, INPUT);
  pinMode(motionPin, INPUT);
  pinMode(relayPin, OUTPUT);
  IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK); 
}

void loop(){
  int gasValue = analogRead(gasPin);
  bool motionDetected = digitalRead(motionPin) == LOW;
  bool flameDetected = digitalRead(flamePin) == HIGH;
  bool hazard = flameDetected || motionDetected || gasValue > 400;

  if (IrReceiver.decode()){
    uint32_t raw = IrReceiver.decodedIRData.decodedRawData;
    Serial.print("Raw code: ");
    Serial.println(raw, HEX);

    if (raw == 0xE9167B80) {
      manualOverride = true;
      overrideState = true;
      Serial.println("Manual override: Access ENABLED.");
    } else if (raw == 0xE8177B80) {
      manualOverride = true;
      overrideState = false;
      Serial.println("Manual override: Access DISABLED.");
    }else if (raw == 0xEC137B80) {
      manualOverride = false;
      Serial.println("Manual ovveride CANCELLED.");
    }
    IrReceiver.resume();
  }

  if (manualOverride) {
    digitalWrite(relayPin, overrideState ? HIGH : LOW);
  } else {
    if (hazard) {
      digitalWrite(relayPin, HIGH);
      Serial.println("Hazard detected! Access DISABLED");
    } else {
      digitalWrite(relayPin, LOW);
      Serial.println("Safe conditions. Access Enabled");
    }
  }
  delay(1000);
}
