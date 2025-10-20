#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <Preferences.h>

// ---------- PIN MAP (edit to match your wiring) ----------
static const int PIN_LORA_SS   = 5;
static const int PIN_LORA_RST  = 14;
static const int PIN_LORA_DIO0 = 26;
static const long LORA_FREQ    = LORA_BAND; // 433E6 from build_flags

// SPI uses SCK=18, MISO=19, MOSI=23 by default on ESP32

// GPS on Serial2
static const int GPS_RX = 16; // ESP32 RX2  (connect to GPS TX)
static const int GPS_TX = 17; // ESP32 TX2  (optional: to GPS RX)

// OLED I2C: SDA=21 SCL=22 (default)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Keypad 4x4 mapping
byte rowPins[4] = {32, 33, 25, 4};    // R1..R4
byte colPins[4] = {27, 12, 13, 15};   // C1..C4

char keys[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};
Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);

// ---------- Globals ----------
TinyGPSPlus gps;
Preferences prefs;              // NVS
HardwareSerial GPSSerial(2);    // Serial2

// UI state (RX screen that exits on D)
enum ScreenState { SCREEN_HOME = 0, SCREEN_RX = 1 };
ScreenState screen = SCREEN_HOME;

// Last known good fix (live)
double curLat = NAN, curLng = NAN;
uint32_t curSat = 0;
double curHdop = NAN;
uint32_t lastFixMillis = 0;

// Persisted (from NVS) if live GPS missing
double nvsLat = NAN, nvsLng = NAN;

// Save cadence
const uint32_t SAVE_INTERVAL = 5UL * 60UL * 1000UL;
uint32_t lastSave = 0;

// NEW: gate periodic saves until first successful save this boot  // <<<
bool firstSaveDone = false;                                        // <<<

// Recent RX text buffers
String lastRx = "";
String rxFull  = "";

// --------- Helpers ---------
void drawHome();

void drawWrappedText(const String& s, int x, int y, uint8_t charsPerLine, uint8_t maxLines) {
  uint16_t start = 0;
  uint8_t line = 0;
  while (start < s.length() && line < maxLines) {
    uint16_t len = min<uint16_t>(charsPerLine, s.length() - start);
    display.setCursor(x, y + line * 8);
    display.print(s.substring(start, start + len));
    start += len;
    line++;
  }
}

void drawRxScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);
  display.println("RECEIVED MESSAGE");
  display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

  drawWrappedText(rxFull, 0, 12, 21, 5);

  display.setCursor(0, 56);
  display.print("Press D to exit");
  display.display();
}

void drawHome() {
  screen = SCREEN_HOME;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("ESP32 LoRa Radio");
  display.println("----------------");

  if (!isnan(curLat) && !isnan(curLng)) {
    display.print("GPS: ");
    display.print(curLat, 6);
    display.print(", ");
    display.println(curLng, 6);
  } else if (!isnan(nvsLat) && !isnan(nvsLng)) {
    display.print("GPS(NVS): ");
    display.print(nvsLat, 6);
    display.print(", ");
    display.println(nvsLng, 6);
  } else {
    display.println("GPS: no fix yet");
  }

  display.println();
  display.println("1: Help");
  display.println("2: Enemy sighted");
  display.println("3: Current location");

  if (lastRx.length()) {
    display.println();
    display.println("RX:");
    display.println(lastRx.substring(0, 21));
  }
  display.display();
}

String formatLocation(double lat, double lng) {
  if (isnan(lat) || isnan(lng)) return String("N/A");
  String s = "";
  s += String(lat, 6);
  s += ",";
  s += String(lng, 6);
  return s;
}

void loadNVS() {
  if (!prefs.begin("gps", true)) {
    Serial.println("Creating new NVS namespace...");
    prefs.begin("gps", false);
    prefs.putDouble("lat", NAN);
    prefs.putDouble("lng", NAN);
  } else {
    nvsLat = prefs.getDouble("lat", NAN);
    nvsLng = prefs.getDouble("lng", NAN);
  }
  prefs.end();
}

void saveNVS(double lat, double lng) {
  prefs.begin("gps", false);
  prefs.putDouble("lat", lat);
  prefs.putDouble("lng", lng);
  prefs.end();
}

bool haveLiveFix() {
  return gps.location.isValid() && gps.location.age() < 5000; // <5s old
}

void updateLiveFix() {
  if (gps.location.isValid()) {
    curLat = gps.location.lat();
    curLng = gps.location.lng();
    curSat = gps.satellites.value();
    curHdop = gps.hdop.hdop();
    lastFixMillis = millis();
  }
}

void handleGPSStream() {
  while (GPSSerial.available()) {
    char c = GPSSerial.read();
    gps.encode(c);
    if (gps.location.isUpdated()) {
      updateLiveFix();
    }
  }
}

String buildMessage(uint8_t which) {
  double lat = NAN, lng = NAN;
  if (haveLiveFix()) { lat = curLat; lng = curLng; }
  else if (!isnan(nvsLat) && !isnan(nvsLng)) { lat = nvsLat; lng = nvsLng; }

  String loc = formatLocation(lat, lng);
  if (which == 1) return "HELP @ " + loc;
  if (which == 2) return "ENEMY SIGHTED @ " + loc;
  return "CURRENT LOCATION @ " + loc;
}

void loraSend(const String& s) {
  Serial.print("[TX] ");
  Serial.println(s);
  LoRa.beginPacket();
  LoRa.print(s);
  LoRa.endPacket();
}

// NEW: perform the one-time first save when a valid fix appears     // <<<
void maybeDoFirstSave() {                                            // <<<
  if (firstSaveDone) return;                                         // <<<
  if (haveLiveFix()) {                                               // <<<
    saveNVS(curLat, curLng);                                         // <<<
    nvsLat = curLat; nvsLng = curLng;                                // <<<
    firstSaveDone = true;                                            // <<<
    lastSave = millis(); // start the 5-min cadence from now         // <<<
    Serial.println("[NVS] First GPS saved");                         // <<<
  }                                                                  // <<<
}                                                                    // <<<

void handleKeypad() {
  char k = keypad.getKey();
  if (!k) return;

  if (screen == SCREEN_RX) {
    if (k == 'D') {
      drawHome();
    }
    return;
  }

  if (k == '1' || k == '2' || k == '3') {
    uint8_t which = (k - '0');
    String msg = buildMessage(which);
    loraSend(msg);
    lastRx = "TX: " + msg;
    drawHome();
  }
}

void handleLoRaRx() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();
    Serial.printf("[RX] %s  RSSI=%d  SNR=%.1f\n",
                incoming.c_str(), LoRa.packetRssi(), LoRa.packetSnr());

    lastRx = incoming;
    rxFull = incoming;

    screen = SCREEN_RX;
    drawRxScreen();
  }
}

// Modified: only run periodic saves AFTER firstSaveDone              // <<<
void maybeSaveFix() {
  if (!firstSaveDone) return;                                        // <<<
  if (millis() - lastSave < SAVE_INTERVAL) return;
  lastSave = millis();

  if (haveLiveFix()) {
    saveNVS(curLat, curLng);
    nvsLat = curLat; nvsLng = curLng;
    Serial.println("[NVS] Periodic GPS saved");
  } else {
    Serial.println("[NVS] Periodic save skipped (no valid fix)");
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(100);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // If OLED init fails, continue headless
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Booting...");
  display.display();

  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  SPI.begin(18, 19, 23); // SCK, MISO, MOSI
  LoRa.setPins(PIN_LORA_SS, PIN_LORA_RST, PIN_LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ)) {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("LoRa init failed!");
    display.display();
  }
  LoRa.setSpreadingFactor(12);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.enableCrc();                // ON on both
  LoRa.setSyncWord(0x34);          // same on both
  LoRa.disableInvertIQ();          // TX and RX must match (default)

  loadNVS();

  // Do NOT mark firstSaveDone from existing NVS; we want a fresh fix first // <<<
  firstSaveDone = false;                                                     // <<<

  drawHome();
}

// ---------- Loop ----------
void loop() {
  handleGPSStream();   // continuously parse GPS data
  maybeDoFirstSave();  // wait for first valid fix then save once     // <<<
  handleKeypad();      // send on 1/2/3 or exit RX on D
  handleLoRaRx();      // show received text (RX screen)
  maybeSaveFix();      // periodic save only after firstSaveDone       // <<<
}
