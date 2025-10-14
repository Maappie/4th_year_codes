#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>

// OLED Setup
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define SCREEN_ADDR  0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Keypad Setup
const byte ROWS = 4;
const byte COLS = 4;
char keys[ROWS][COLS] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
// Keypad wiring (change if you use different pins)
byte rowPins[ROWS] = {14, 12, 13, 4}; // R1-R4
byte colPins[COLS] = {32, 33, 25, 26}; // C1-C4

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

// Mapping table: key to button number (for display)
int getButtonNumber(char key) {
  switch (key) {
    case '1': return 1;
    case '2': return 2;
    case '3': return 3;
    case 'A': return 10;
    case '4': return 4;
    case '5': return 5;
    case '6': return 6;
    case 'B': return 11;
    case '7': return 7;
    case '8': return 8;
    case '9': return 9;
    case 'C': return 12;
    case '*': return 13;
    case '0': return 14;
    case '#': return 15;
    case 'D': return 16;
    default: return 0;
  }
}

void showText(const String& text) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(2);
  display.setCursor(0, (SCREEN_HEIGHT-16)/2);
  display.print(text);
  display.display();
}

void setup() {
  Serial.begin(115200);
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    Serial.println("SSD1306 init failed!");
    for (;;);
  }
  display.clearDisplay();
  display.display();
  showText("Ready");
}

void loop() {
  char key = keypad.getKey();
  if (key) {
    Serial.print("Key: "); Serial.println(key);

    String msg = String("Key ") + key + " pressed";
    showText(msg);
    Serial.println(msg);

    delay(400);
    showText("Ready");
  }
}
