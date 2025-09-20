#include <Arduino.h>
#include <ESP32Servo.h>

// Pins
const int SERVO_PIN = 18;
const int JOY_Y_PIN = 34;

Servo myServo;

// Calibration values
int joyMin = 0;
int joyCenter = 2048;
int joyMax = 4095;

const int DEADZONE = 250; // adjust bigger if still twitchy

void setup() {
  Serial.begin(115200);
  delay(100);

  analogSetPinAttenuation(JOY_Y_PIN, ADC_11db);
  myServo.attach(SERVO_PIN);
  myServo.write(90);

  // --- Calibrate Center ---
  long sum = 0;
  for (int i = 0; i < 300; i++) {
    sum += analogRead(JOY_Y_PIN);
    delay(3);
  }
  joyCenter = sum / 300;
  Serial.print("Center = ");
  Serial.println(joyCenter);

  // --- Find Min & Max ---
  Serial.println("Move joystick FULL DOWN and FULL UP now (5 sec)...");
  long minVal = 4095;
  long maxVal = 0;
  unsigned long start = millis();
  while (millis() - start < 5000) {
    int val = analogRead(JOY_Y_PIN);
    if (val < minVal) minVal = val;
    if (val > maxVal) maxVal = val;
    delay(5);
  }
  joyMin = minVal;
  joyMax = maxVal;

  Serial.print("Min = ");
  Serial.print(joyMin);
  Serial.print("  Center = ");
  Serial.print(joyCenter);
  Serial.print("  Max = ");
  Serial.println(joyMax);

  Serial.println("Calibration done.");
}

void loop() {
  int joyY = analogRead(JOY_Y_PIN);
  int angle;

  // --- Deadzone check ---
  if (abs(joyY - joyCenter) <= DEADZONE) {
    angle = 90;  // lock to center
  }
  else if (joyY > joyCenter) {
    // Map [center+deadzone .. max] -> [90 .. 180]
    angle = map(joyY, joyCenter + DEADZONE, joyMax, 90, 180);
  }
  else {
    // Map [min .. center-deadzone] -> [0 .. 90]
    angle = map(joyY, joyMin, joyCenter - DEADZONE, 0, 90);
  }

  angle = constrain(angle, 0, 180);
  myServo.write(angle);

  Serial.print("joyY=");
  Serial.print(joyY);
  Serial.print("  angle=");
  Serial.println(angle);

  delay(40);
}
