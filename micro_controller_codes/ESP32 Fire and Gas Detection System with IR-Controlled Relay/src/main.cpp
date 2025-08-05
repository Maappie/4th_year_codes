#include <Arduino.h>
#include <IRremoteESP8266.h>
#include <IRrecv.h>
#include <IRutils.h>

// === PIN DEFINITIONS (adjust as needed) ===
const int flamePin = 15;   // Flame sensor input
const int gasPin = 34;     // Gas sensor analog input (use ADC1 pin)
const int relayPin = 4;    // Relay output
const int irPin = 14;      // IR receiver input (any digital pin, not 34–39)

IRrecv irrecv(irPin);
decode_results results;

void setup() {
  Serial.begin(115200);

  pinMode(flamePin, INPUT);
  pinMode(relayPin, OUTPUT);

  irrecv.enableIRIn();  // Start IR receiver
  Serial.println("ESP32 Fire & Gas Detection System Ready");
}

void loop() {
  int gasValue = analogRead(gasPin);
  bool flameDetected = digitalRead(flamePin) == LOW; // LOW = flame detected

  // Auto-trigger relay if danger is detected
  if (flameDetected || gasValue > 400) {
    digitalWrite(relayPin, HIGH);
    Serial.println("🔥 Danger detected! Relay ON");
  } else {
    digitalWrite(relayPin, LOW);
  }

  // Manual control via IR remote
  if (irrecv.decode(&results)) {
    Serial.printf("IR Code: 0x%X\n", results.value);

    if (results.value == 0xFF30CF) {          // Power button (example)
      digitalWrite(relayPin, HIGH);
      Serial.println("IR: Relay ON");
    } else if (results.value == 0xFF18E7) {   // Play/Pause button (example)
      digitalWrite(relayPin, LOW);
      Serial.println("IR: Relay OFF");
    }

    irrecv.resume(); // Ready for next signal
  }

  delay(100);
}
