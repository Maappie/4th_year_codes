#include <SevSeg.h>
SevSeg sevseg;

void setup() {
  byte numDigits = 4;

  // Digits D1..D4 (left to right)
  byte digitPins[]   = {10, 11, 12, 13};

  // Segments a,b,c,d,e,f,g,dp
  byte segmentPins[] = {2, 3, 4, 5, 6, 7, 8, 9};

  bool resistorsOnSegments = true;      // resistors on segment lines
  byte hardwareConfig = COMMON_CATHODE; // 5641AS is common cathode
  bool updateWithDelays = false;
  bool leadingZeros = true;             // <--- ENABLE leading zeros
  bool disableDecPoint = false;

  sevseg.begin(hardwareConfig, numDigits, digitPins, segmentPins,
               resistorsOnSegments, updateWithDelays, leadingZeros, disableDecPoint);
  sevseg.setBrightness(90);
}

void loop() {
  static int counter = 1;
  static unsigned long lastUpdate = 0;

  if (millis() - lastUpdate >= 1000) {
    sevseg.setNumber(counter);
    counter++;
    if (counter > 10) counter = 1; // loop from 0001 → 0010
    lastUpdate = millis();
  }

  sevseg.refreshDisplay();   // must run continuously
}
