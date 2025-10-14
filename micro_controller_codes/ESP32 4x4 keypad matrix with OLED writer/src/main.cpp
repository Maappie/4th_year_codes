#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

// ---------- OLED Setup ----------
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define SCREEN_ADDR  0x3C

// ESP32 I2C pins
#define SDA_PIN 21
#define SCL_PIN 22

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---------- 4x4 Keypad Setup ----------
const byte ROWS = 4;
const byte COLS = 4;

// Layout:
// R1: 1 2 3 A
// R2: 4 5 6 B
// R3: 7 8 9 C
// R4: * 0 # D
char keys4x4[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

// Your wiring, but R4 moved 15 -> 4 to avoid boot issues
byte rowPins[ROWS] = {14, 12, 13, 4};     // R1..R4 (GPIO4 instead of 15)
byte colPins[COLS] = {32, 33, 25, 26};    // C1..C4

Keypad keypad = Keypad(makeKeymap(keys4x4), rowPins, colPins, ROWS, COLS);

// ---------- Typing Buffer ----------
static String buffer = "";
static const size_t MAX_BUF = 168; 

// Allow typing these. Ignore '*'.
bool isTypable(char k) {
  if ((k >= '0' && k <= '9') || k == '#' || k == '*') return true;
  if (k == 'A' || k == 'B' || k == 'C' || k == 'D') return true;
  return false;
}

// Append with cap; keep the most recent characters
void appendChar(char k) {
  buffer += k;
  if (buffer.length() > MAX_BUF) {
    // drop oldest extra chars
    buffer.remove(0, buffer.length() - MAX_BUF);
  }
}

// Render buffer with simple wrapping at text size 2
void showBuffer() {
  // Size-2 font: ~12 px wide per char, 16 px high
  const uint8_t textSize = 1;
  const uint8_t charW = 6 * textSize;   // GFX base 6x8
  const uint8_t charH = 8 * textSize;
  const uint8_t perLine = SCREEN_WIDTH / charW;   // ~10 chars
  const uint8_t maxLines = SCREEN_HEIGHT / charH; // 4 lines

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(textSize);

  uint8_t line = 0, col = 0;
  int16_t x = 0, y = 0;

  // Only render the tail that fits (optional optimization)
  // Here we just iterate full buffer and wrap; older chars may scroll off top silently.
  for (size_t i = 0; i < buffer.length(); ++i) {
    char c = buffer[i];
    // wrap on width
    if (col >= perLine) {
      col = 0;
      line++;
    }
    // stop if no more vertical space
    if (line >= maxLines) break;

    x = col * charW;
    y = line * charH;
    display.setCursor(x, y);
    display.write(c);
    col++;
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  Wire.begin(SDA_PIN, SCL_PIN);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    Serial.println("SSD1306 init failed!");
    while (1) { delay(10); }
  }
  display.clearDisplay();
  display.display();

  buffer.reserve(MAX_BUF + 8);
  buffer = "";  // start empty
  showBuffer();
}

void loop() {
  char key = keypad.getKey();
  if (key) {
    if (isTypable(key)) {
      appendChar(key);
      Serial.print("Typed: "); Serial.println(key);
      showBuffer();
      delay(10); // light de-bounce / repeat suppression for typing feel
    }
    // '*' is ignored; no other controls for now.
  }
}
