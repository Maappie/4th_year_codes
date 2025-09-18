// ===== 2WD Line Follower (x1x Unlock, 000 Moves Forward, ESP32 + L298N, Reverse Turning) =====
#include <Arduino.h>

// -------------------- Motor Driver (L298N) --------------------
#define IN1 26
#define IN2 27
#define IN3 32
#define IN4 33

// -------------------- IR Sensors --------------------
#define IR_LEFT    34
#define IR_CENTER  25
#define IR_RIGHT   35

// -------------------- Constants --------------------
const int BASE_SPEED = 2230;          // Speed when going forward
const int TURN_SPEED = 180;          // Forward motor speed during turn
const int TURN_REVERSE_SPEED = 110;  // Reverse motor speed during turn
const int MAX_PWM = 255;
const uint16_t LOOP_DELAY_MS = 3;

const int PWM_FREQ = 20000, PWM_RES = 8;
const int PWM_CH_R_FWD = 0, PWM_CH_R_REV = 1, PWM_CH_L_FWD = 2, PWM_CH_L_REV = 3;

// -------------------- Enums + Globals --------------------
enum Mode { FOLLOW, FIX_LEFT, FIX_RIGHT };
Mode mode = FOLLOW;

// -------------------- Motor Helpers --------------------
inline void writeMotor(int pwm, int fwd_ch, int rev_ch) {
  if (pwm >= 0) {
    ledcWrite(fwd_ch, pwm);
    ledcWrite(rev_ch, 0);
  } else {
    ledcWrite(fwd_ch, 0);
    ledcWrite(rev_ch, -pwm);
  }
}

inline void setMotor(int L, int R) {
  L = constrain(L, -MAX_PWM, MAX_PWM);
  R = constrain(R, -MAX_PWM, MAX_PWM);
  writeMotor(L, PWM_CH_L_FWD, PWM_CH_L_REV);
  writeMotor(R, PWM_CH_R_FWD, PWM_CH_R_REV);
}

inline void stopMotors() {
  setMotor(0, 0);
}

// -------------------- Sensor Reading --------------------
inline uint8_t norm(int raw) { return raw == HIGH ? 1 : 0; }

struct Reading {
  uint8_t L, C, R, pat;
};

Reading readSensors() {
  uint8_t Lc = 0, Cc = 0, Rc = 0;
  for (int i = 0; i < 3; i++) {
    Lc += norm(digitalRead(IR_LEFT));
    Cc += norm(digitalRead(IR_CENTER));
    Rc += norm(digitalRead(IR_RIGHT));
    delayMicroseconds(350);
  }
  uint8_t L = Lc >= 2, C = Cc >= 2, R = Rc >= 2;
  uint8_t pat = static_cast<uint8_t>((L << 2) | (C << 1) | R);
  return {L, C, R, pat};
}

// -------------------- Setup --------------------
void setup() {
  Serial.begin(115200);
  for (int pin : {IN1, IN2, IN3, IN4}) pinMode(pin, OUTPUT);
  for (int pin : {IR_LEFT, IR_CENTER, IR_RIGHT}) pinMode(pin, INPUT);

  ledcSetup(PWM_CH_R_FWD, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_R_REV, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_L_FWD, PWM_FREQ, PWM_RES);
  ledcSetup(PWM_CH_L_REV, PWM_FREQ, PWM_RES);

  ledcAttachPin(IN1, PWM_CH_R_FWD); ledcAttachPin(IN2, PWM_CH_R_REV);
  ledcAttachPin(IN3, PWM_CH_L_FWD); ledcAttachPin(IN4, PWM_CH_L_REV);
  stopMotors(); delay(300);
  Serial.println("[READY] Line Follower with Reverse Turning (x1x unlock, 000 forward)");
}

// -------------------- Main Loop --------------------
void loop() {
  Reading rd = readSensors();
  int lPWM = 0, rPWM = 0;

  if (mode == FOLLOW) {
    if (rd.C == 1 || rd.pat == 0b000) {
      // Go forward even if totally lost (000)
      lPWM = rPWM = BASE_SPEED;
    } else if (rd.L == 1) {
      mode = FIX_LEFT;
    } else if (rd.R == 1) {
      mode = FIX_RIGHT;
    }
  }

  else if (mode == FIX_LEFT) {
    lPWM = -TURN_REVERSE_SPEED;  // Left motor goes in reverse
    rPWM = TURN_SPEED;           // Right motor goes forward
    if (rd.C == 1) {
      mode = FOLLOW;
      lPWM = rPWM = BASE_SPEED;
    }
  }

  else if (mode == FIX_RIGHT) {
    lPWM = TURN_SPEED;            // Left motor goes forward
    rPWM = -TURN_REVERSE_SPEED;  // Right motor goes in reverse
    if (rd.C == 1) {
      mode = FOLLOW;
      lPWM = rPWM = BASE_SPEED;
    }
  }

  Serial.print("[MODE:");
  Serial.print((mode == FOLLOW) ? "FOLLOW" : (mode == FIX_LEFT) ? "FIX_LEFT" : "FIX_RIGHT");
  Serial.print("] [LCR:"); Serial.print(rd.L); Serial.print(rd.C); Serial.print(rd.R);
  Serial.print("] [PAT:"); Serial.print(rd.pat, BIN);
  Serial.print("] [PWM L:"); Serial.print(lPWM); Serial.print(" R:"); Serial.print(rPWM); Serial.println("]");

  setMotor(lPWM, rPWM);
  delay(LOOP_DELAY_MS);
}
