#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
  #include <espnow.h>
}

// -------------------- Motor Command Peer --------------------
uint8_t esp32Address[] = {0x44, 0x1D, 0x64, 0xF3, 0x35, 0xDC}; // Replace with your ESP32 MAC

// -------------------- Joystick & Buttons --------------------
#define JOY_Y     A0      // Forward/Reverse
#define BTN_LEFT  D5      // Left button
#define BTN_RIGHT D6      // Right button
#define BTN_SERVO D7      // Toggle servo button

// -------------------- Constants --------------------
const int MAX_PWM = 255;
const int DEADZONE_Y = 50;  // deadzone around center
int joyCenterY = 512;       // will be calibrated

// -------------------- Servo Toggle Values --------------------
const int SERVO_TOGGLE_LOW  = 5;   // default position
const int SERVO_TOGGLE_HIGH = 80;  // toggled position

// -------------------- Globals --------------------
struct MotorCommand {
  int16_t leftPWM;   
  int16_t rightPWM;  
  int16_t servo1Angle;  
  int16_t servo2Angle;  
};

int16_t servo1Angle = SERVO_TOGGLE_LOW;   // default servo position
bool lastServoBtnState = HIGH; // for edge detection

// -------------------- Helpers --------------------
void sendMotorCommand(int16_t leftPWM, int16_t rightPWM) {
  MotorCommand cmd;
  cmd.leftPWM = leftPWM;
  cmd.rightPWM = rightPWM;
  cmd.servo1Angle = servo1Angle; // send current servo angle
  cmd.servo2Angle = servo1Angle; // make second servo follow same angle
  esp_now_send(esp32Address, (uint8_t *)&cmd, sizeof(cmd));
}

void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("📤 Send Status: ");
  Serial.println(sendStatus == 0 ? "Success" : "Fail");
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(9600);

  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_SERVO, INPUT_PULLUP);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  wifi_set_channel(1);

  Serial.print("📡 ESP8266 STA MAC: ");
  Serial.println(WiFi.macAddress());
  Serial.print("📡 ESP8266 channel: ");
  Serial.println(wifi_get_channel());

  if (esp_now_init() != 0) {
    Serial.println("❌ ESP-NOW init failed!");
    return;
  }

  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(OnDataSent);

  if (esp_now_add_peer(esp32Address, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
    Serial.println("❌ Failed to add peer");
    return;
  }
  Serial.println("✅ ESP32 peer added");

  // -------------------- Calibrate joystick center --------------------
  long sumY = 0;
  for (int i = 0; i < 200; i++) {
    sumY += analogRead(JOY_Y);
    delay(2);
  }
  joyCenterY = sumY / 200;
  Serial.print("Joystick Center Y = "); Serial.println(joyCenterY);
}

// -------------------- Main Loop --------------------
void loop() {
  int rawY = analogRead(JOY_Y);
  int offsetY = rawY - joyCenterY;

  // Apply deadzone
  if (abs(offsetY) < DEADZONE_Y) offsetY = 0;

  // Read buttons (active LOW)
  bool leftBtn = digitalRead(BTN_LEFT) == LOW;
  bool rightBtn = digitalRead(BTN_RIGHT) == LOW;

  // -------------------- Servo Toggle --------------------
  bool servoBtnState = digitalRead(BTN_SERVO);
  if (lastServoBtnState == HIGH && servoBtnState == LOW) { // button press detected
    servo1Angle = (servo1Angle == SERVO_TOGGLE_LOW) ? SERVO_TOGGLE_HIGH : SERVO_TOGGLE_LOW;
    Serial.print("🔄 Servo toggled to: ");
    Serial.println(servo1Angle);
  }
  lastServoBtnState = servoBtnState;

  int16_t leftPWM = 0;
  int16_t rightPWM = 0;

  // -------------------- Movement Logic --------------------
  if (offsetY < 0) { // push-up -> forward (inverted logic)
    if (!leftBtn && !rightBtn) {         // full forward
      leftPWM = MAX_PWM; 
      rightPWM = MAX_PWM; 
    } else if (leftBtn && !rightBtn) {   // forward + left
      leftPWM = MAX_PWM / 2;
      rightPWM = MAX_PWM;
    } else if (!leftBtn && rightBtn) {   // forward + right
      leftPWM = MAX_PWM;
      rightPWM = MAX_PWM / 2;
    }
  } 
  else if (offsetY > 0) { // push-down -> reverse (inverted logic)
    if (!leftBtn && !rightBtn) {         // full reverse
      leftPWM = -MAX_PWM;
      rightPWM = -MAX_PWM;
    } else if (leftBtn && !rightBtn) {   // reverse + left
      leftPWM = -MAX_PWM / 2;
      rightPWM = -MAX_PWM;
    } else if (!leftBtn && rightBtn) {   // reverse + right
      leftPWM = -MAX_PWM;
      rightPWM = -MAX_PWM / 2;
    }
  } 
  else { // joystick centered
    if (leftBtn && !rightBtn) {          // hard left turn
      leftPWM = -MAX_PWM;
      rightPWM = MAX_PWM;
    } else if (!leftBtn && rightBtn) {   // hard right turn
      leftPWM = MAX_PWM;
      rightPWM = -MAX_PWM;
    } else {                              // stop
      leftPWM = 0;
      rightPWM = 0;
    }
  }

  // -------------------- Send Motor Command --------------------
  sendMotorCommand(leftPWM, rightPWM);

  // -------------------- Debug --------------------
  Serial.print("rawY="); Serial.print(rawY);
  Serial.print(" | offsetY="); Serial.print(offsetY);
  Serial.print(" | L="); Serial.print(leftPWM);
  Serial.print(" R="); Serial.print(rightPWM);
  Serial.print(" | Servo="); Serial.println(servo1Angle);
  
  delay(20); // small delay to avoid spamming ESP-NOW
}
