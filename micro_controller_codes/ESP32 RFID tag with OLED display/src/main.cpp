#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---------------- PIN DEFINITIONS ----------------
#define SS_PIN   5     // RC522 SDA
#define RST_PIN  27    // RC522 RST
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define SCREEN_ADDR  0x3C   // Change to 0x3D if OLED stays blank

// ---------------- OBJECTS ----------------
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MFRC522 mfrc522(SS_PIN, RST_PIN);

// ---------------- CARD UIDs ----------------
const char* UID_BLUE  = "E1:4D:90:04";     
const char* UID_WHITE = "83:36:50:E4";   

// ---------------- FUNCTIONS ----------------
String uidToString(const MFRC522::Uid &uid) {
  String s;
  for (byte i = 0; i < uid.size; i++) {
    if (uid.uidByte[i] < 0x10) s += "0";
    s += String(uid.uidByte[i], HEX);
    if (i + 1 < uid.size) s += ":";
  }
  s.toUpperCase();
  return s;
}

void showLine(const char* text) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (SCREEN_WIDTH - w) / 2;
  int16_t y = (SCREEN_HEIGHT - h) / 2;

  display.setCursor(x, y);
  display.println(text);
  display.display();
}

// ---------------- SETUP ----------------
void setup() {
  Serial.begin(115200);
  while (!Serial) {}

  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    Serial.println("SSD1306 init failed!");
    for (;;);
  }
  display.clearDisplay();
  display.display();

  // Initialize RC522
  SPI.begin();  // ESP32 default: SCK=18, MISO=19, MOSI=23
  mfrc522.PCD_Init(SS_PIN, RST_PIN);
  delay(50);

  showLine("Ready");
  Serial.println("Ready. Tap your tag or card.");
}

// ---------------- LOOP ----------------
void loop() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial())
    return;

  String uid = uidToString(mfrc522.uid);
  Serial.print("UID: ");
  Serial.println(uid);

  if (strcasecmp(uid.c_str(), UID_BLUE) == 0) {
    showLine("Hello!");
  } else if (strcasecmp(uid.c_str(), UID_WHITE) == 0) {
    showLine("Good morning!");
  } else {
    showLine("Unknown");
  }

  // Halt card
  mfrc522.PICC_HaltA();
  mfrc522.PCD_StopCrypto1();

  delay(1500);
  showLine("Ready");
}
