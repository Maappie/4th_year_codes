#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <LoRa.h>
#include <TinyGPSPlus.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Keypad.h>
#include <Preferences.h>
#include <memory>   

// --- mbedTLS (bundled with ESP32 Arduino) ---
#include "mbedtls/sha256.h"
#include "mbedtls/gcm.h"

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
Preferences prefs;              // reused for multiple namespaces
HardwareSerial GPSSerial(2);    // Serial2

// UI state (RX screen that exits on D)
enum ScreenState { SCREEN_HOME = 0, SCREEN_RX = 1, SCREEN_LOG_LIST = 2, SCREEN_LOG_VIEW = 3 };
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

// Gate periodic saves until first successful save this boot
bool firstSaveDone = false;

// Recent RX text buffers (for the RX popup screen only)
String lastRx = "";
String rxFull  = "";

// --------- CRYPTO CONSTANTS / HELPERS ---------
static const char* TX_SENDER_TAG = "Command Center";
// TX key label; RX key labels:
static const char* TX_KEY_LABEL  = "HQ-PH";
static const char* RX_KEYS_LABELS[] = {"unit-001", "unit-002"};
static const size_t RX_KEYS_COUNT = sizeof(RX_KEYS_LABELS)/sizeof(RX_KEYS_LABELS[0]);

// AES-GCM params
static const size_t KEY_LEN   = 32;  // 256-bit (SHA256 output)
static const size_t NONCE_LEN = 12;  // 96-bit nonce (GCM)
static const size_t TAG_LEN   = 16;  // 128-bit tag

struct KeyEntry {
  uint8_t key[KEY_LEN];
  const char* label;
};

KeyEntry txKey;
KeyEntry rxKeys[RX_KEYS_COUNT];

// ---------- PERSISTENT LAST-10 MESSAGE LOG (NVS) ----------
struct MsgEntry {
  String sender;
  String msg;
  String nonceHex;
};

// In-RAM shadow of NVS log
const int LOG_CAP = 10;
MsgEntry logBuf[LOG_CAP];
uint8_t logCount = 0;  // number of valid entries (0..10)
uint8_t logHead  = 0;  // index of next write position (0..9)

// For log list paging: 0 = show 0..4, 1 = show 5..9
uint8_t logListPage = 0;

// NVS helpers for the message log
void loadMsgLogFromNVS() {
  if (!prefs.begin("msglog", true)) return;
  logCount = prefs.getUChar("cnt", 0);
  logHead  = prefs.getUChar("head", 0);
  if (logCount > LOG_CAP) logCount = LOG_CAP;
  if (logHead >= LOG_CAP) logHead = 0;

  for (int i = 0; i < LOG_CAP; ++i) {
    String sKey = String("s") + i;
    String mKey = String("m") + i;
    String nKey = String("n") + i;
    logBuf[i].sender   = prefs.getString(sKey.c_str(), "");
    logBuf[i].msg      = prefs.getString(mKey.c_str(), "");
    logBuf[i].nonceHex = prefs.getString(nKey.c_str(), "");
  }
  prefs.end();
}

void saveMsgLogSlotToNVS(int slot) {
  if (!prefs.begin("msglog", false)) return;
  String sKey = String("s") + slot;
  String mKey = String("m") + slot;
  String nKey = String("n") + slot;
  prefs.putString(sKey.c_str(), logBuf[slot].sender);
  prefs.putString(mKey.c_str(), logBuf[slot].msg);
  prefs.putString(nKey.c_str(), logBuf[slot].nonceHex);
  prefs.end();
}

void saveMsgLogMetaToNVS() {
  if (!prefs.begin("msglog", false)) return;
  prefs.putUChar("cnt",  logCount);
  prefs.putUChar("head", logHead);
  prefs.end();
}

// Add new entry (authenticated RX). Newest is index 0 for the user view.
void addLogEntryPersistent(const String& sender, const String& message, const String& nonceHex) {
  logBuf[logHead].sender   = sender;
  logBuf[logHead].msg      = message;
  logBuf[logHead].nonceHex = nonceHex;

  saveMsgLogSlotToNVS(logHead);

  logHead = (logHead + 1) % LOG_CAP;
  if (logCount < LOG_CAP) logCount++;
  saveMsgLogMetaToNVS();
}

// Map a user-facing index (0=newest .. 9=oldest) to the circular slot.
// Returns -1 if that index is out of range for current logCount.
int userIndexToSlot(int userIdx) {
  if (userIdx < 0 || userIdx >= logCount) return -1;
  int newestSlot = (int)logHead - 1;
  if (newestSlot < 0) newestSlot += LOG_CAP;
  int slot = newestSlot - userIdx;
  while (slot < 0) slot += LOG_CAP;
  return slot % LOG_CAP;
}

// --------- UI Helpers ---------
void drawHome();
void drawLogListScreen();
void drawLogViewScreen(int selectedSlot);

String truncate21(const String& s) {
  if (s.length() <= 21) return s;
  return s.substring(0, 20) + "…";
}

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

  drawWrappedText(rxFull, 0, 12, 21, 5);

  display.setCursor(0, 56);
  display.print("Press D to exit");
  display.display();
}

// --- HOME SCREEN: revised per request ---
void drawHome() {
  screen = SCREEN_HOME;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Title only (no underline)
  display.setCursor(0,0);
  display.println(TX_SENDER_TAG);

  // GPS status only (no coordinates)
  display.setCursor(0, 12);
  // Decide status:
  // - updated: live fix and refreshed within last 5 minutes
  // - not updated: we have a fix (live old or only NVS), but not fresh in last 5 minutes
  // - not fix: no live fix and no stored coords
  bool haveStored = (!isnan(nvsLat) && !isnan(nvsLng));
  bool live = gps.location.isValid();
  bool fresh = live && (millis() - lastFixMillis < SAVE_INTERVAL);

  if (live && fresh) {
    display.println("GPS: updated");
  } else if (live || haveStored) {
    display.println("GPS: not updated");
  } else {
    display.println("GPS: not fix");
  }

  display.setCursor(0, 24);
  display.println("1: Help");
  display.println("2: Enemy sighted");
  display.println("3: Current location");
  display.println("C: View saved msgs");

  display.display();
}

// --- LOG LIST: paged 0-4 or 5-9, no underline under header ---
void drawLogListScreen() {
  screen = SCREEN_LOG_LIST;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("SELECT MSG 0-9");

  // Which range to show
  int startIdx = (logListPage == 0) ? 0 : 5;
  int endIdx   = startIdx + 5; // exclusive

  // One-column list to avoid overlaps
  for (int i = startIdx; i < endIdx; ++i) {
    int slot = userIndexToSlot(i);
    String label = String(i) + String(". ");
    if (slot >= 0) label += truncate21(logBuf[slot].sender);
    else           label += "(empty)";
    int line = i - startIdx;
    display.setCursor(0, 12 + line * 10);
    display.println(label);
  }

  // Footer (keep minimal to avoid crowding)
  display.setCursor(0, 56);
  display.print("#: Next  D: Back");
  display.display();
}

void drawLogViewScreen(int selectedSlot) {
  screen = SCREEN_LOG_VIEW;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);
  display.print("From: ");
  display.println(truncate21(logBuf[selectedSlot].sender));

  display.setCursor(0, 12);
  drawWrappedText(logBuf[selectedSlot].msg, 0, 12, 21, 5);

  display.setCursor(0, 56);
  display.print("D: Back");
  display.display();
}

// ---------- GPS / NVS (GPS) ----------
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

String formatLocation(double lat, double lng) {
  if (isnan(lat) || isnan(lng)) return String("N/A");
  String s = "";
  s += String(lat, 6);
  s += ",";
  s += String(lng, 6);
  return s;
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

// ---------- CRYPTO ----------
static char nybble(uint8_t v){ return v < 10 ? '0'+v : 'a'+(v-10); }
String toHex(const uint8_t* data, size_t len){
  String s; s.reserve(len*2);
  for(size_t i=0;i<len;i++){ s += nybble((data[i]>>4)&0x0F); s += nybble(data[i]&0x0F); }
  return s;
}
bool fromHex(const String& hex, uint8_t* out, size_t outLen){
  if(hex.length() != (int)(outLen*2)) return false;
  auto hexVal = [](char c)->int{
    if(c>='0'&&c<='9') return c-'0';
    if(c>='a'&&c<='f') return 10+(c-'a');
    if(c>='A'&&c<='F') return 10+(c-'A');
    return -1;
  };
  for(size_t i=0;i<outLen;i++){
    int hi = hexVal(hex[2*i]); int lo = hexVal(hex[2*i+1]);
    if(hi<0||lo<0) return false;
    out[i] = (uint8_t)((hi<<4)|lo);
  }
  return true;
}

void deriveKey(const char* label, uint8_t outKey[KEY_LEN]){
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0); // 0 = SHA-256
  mbedtls_sha256_update_ret(&ctx, (const unsigned char*)label, strlen(label));
  mbedtls_sha256_finish_ret(&ctx, outKey);
  mbedtls_sha256_free(&ctx);
}

void makeNonce(uint8_t nonce[NONCE_LEN]){
  for(size_t i=0;i<NONCE_LEN;i+=4){
    uint32_t r = esp_random();
    size_t chunk = min((size_t)4, NONCE_LEN - i);
    memcpy(nonce+i, &r, chunk);
  }
}

bool aesGcmEncrypt(const uint8_t key[KEY_LEN],
                   const uint8_t* plaintext, size_t plen,
                   const uint8_t* nonce, size_t nlen,
                   const uint8_t* aad, size_t aad_len,
                   uint8_t* ciphertext,
                   uint8_t* tag, size_t tlen){
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN*8);
  if(rc!=0){ mbedtls_gcm_free(&gcm); return false; }
  rc = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plen,
                                 nonce, nlen,
                                 aad, aad_len,
                                 plaintext, ciphertext,
                                 tlen, tag);
  mbedtls_gcm_free(&gcm);
  return rc==0;
}

bool aesGcmDecrypt(const uint8_t key[KEY_LEN],
                   const uint8_t* ciphertext, size_t clen,
                   const uint8_t* nonce, size_t nlen,
                   const uint8_t* aad, size_t aad_len,
                   const uint8_t* tag, size_t tlen,
                   uint8_t* plaintext_out){
  mbedtls_gcm_context gcm;
  mbedtls_gcm_init(&gcm);
  int rc = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, KEY_LEN*8);
  if(rc!=0){ mbedtls_gcm_free(&gcm); return false; }
  rc = mbedtls_gcm_auth_decrypt(&gcm, clen,
                                nonce, nlen,
                                aad, aad_len,
                                tag, tlen,
                                ciphertext, plaintext_out);
  mbedtls_gcm_free(&gcm);
  return rc==0;
}

// ---------- ENCRYPTED TX / RX ----------
bool loraSendEncrypted(const String& plaintext){
  uint8_t nonce[NONCE_LEN]; 
  makeNonce(nonce);

  // Show nonce (hex) on TX
  String nHex = toHex(nonce, NONCE_LEN);
  Serial.print("[TX NONCE] ");
  Serial.println(nHex);

  // Prepare buffers
  const size_t plen = plaintext.length();
  std::unique_ptr<uint8_t[]> pbuf(new uint8_t[plen]);
  memcpy(pbuf.get(), plaintext.c_str(), plen);

  std::unique_ptr<uint8_t[]> cbuf(new uint8_t[plen]);
  uint8_t tag[TAG_LEN];

  const uint8_t* aad = (const uint8_t*)TX_SENDER_TAG;
  size_t aad_len = strlen(TX_SENDER_TAG);

  bool ok = aesGcmEncrypt(txKey.key, pbuf.get(), plen, nonce, NONCE_LEN, aad, aad_len, cbuf.get(), tag, TAG_LEN);
  if(!ok){
    Serial.println("[ENC] AES-GCM encrypt failed");
    return false;
  }

  // Compose ASCII frame
  String frame = "ENC|tag="; frame += TX_SENDER_TAG;
  frame += "|n="; frame += nHex;
  frame += "|c="; frame += toHex(cbuf.get(), plen);
  frame += "|t="; frame += toHex(tag, TAG_LEN);

  Serial.print("[TX] "); Serial.println(frame);
  LoRa.beginPacket();
  LoRa.print(frame);
  LoRa.endPacket();
  return true;
}

bool tryDecryptWithKey(const KeyEntry& k, const String& senderTag,
                       const String& nonceHex, const String& cHex, const String& tHex,
                       String& outPlain){
  // Decode fields
  uint8_t nonce[NONCE_LEN];
  if(!fromHex(nonceHex, nonce, NONCE_LEN)) return false;

  const size_t clen = cHex.length()/2;
  std::unique_ptr<uint8_t[]> cbuf(new uint8_t[clen]);
  if(!fromHex(cHex, cbuf.get(), clen)) return false;

  uint8_t tag[TAG_LEN];
  if(!fromHex(tHex, tag, TAG_LEN)) return false;

  std::unique_ptr<uint8_t[]> pbuf(new uint8_t[clen]); // plaintext len == ciphertext len for GCM

  const uint8_t* aad = (const uint8_t*)senderTag.c_str();
  size_t aad_len = senderTag.length();

  bool ok = aesGcmDecrypt(k.key, cbuf.get(), clen, nonce, NONCE_LEN, aad, aad_len, tag, TAG_LEN, pbuf.get());
  if(!ok) return false;

  outPlain = String((const char*)pbuf.get(), clen);
  return true;
}

bool parseEncryptedFrame(const String& s, String& senderTag, String& nHex, String& cHex, String& tHex){
  // Format: ENC|tag=...|n=...|c=...|t=...
  if(!s.startsWith("ENC|")) return false;
  int p1 = s.indexOf("|tag=");
  int p2 = s.indexOf("|n=");
  int p3 = s.indexOf("|c=");
  int p4 = s.indexOf("|t=");
  if(p1 < 0 || p2 < 0 || p3 < 0 || p4 < 0) return false;

  int tagStart = p1 + 5;
  senderTag = s.substring(tagStart, p2);

  int nStart = p2 + 3;
  nHex = s.substring(nStart, p3);

  int cStart = p3 + 3;
  cHex = s.substring(cStart, p4);

  int tStart = p4 + 3;
  tHex = s.substring(tStart);
  return true;
}

void loraSend(const String& s) {
  if(loraSendEncrypted(s)){
    Serial.print("[TX PLAINTEXT] "); Serial.println(s);
  }else{
    Serial.println("[WARN] Fallback to plaintext TX");
    LoRa.beginPacket();
    LoRa.print(s);
    LoRa.endPacket();
  }
}

// ---------- Keypad ----------
void drawLogListScreen(); // forward (already declared above)

void handleKeypad() {
  char k = keypad.getKey();
  if (!k) return;

  // Route by screen
  switch (screen) {
    case SCREEN_RX:
      if (k == 'D') drawHome();
      return;

    case SCREEN_LOG_LIST:
      if (k == 'D') { drawHome(); return; }
      if (k == '#') { 
        // toggle page 0<->1 and redraw
        logListPage ^= 1; 
        drawLogListScreen(); 
        return; 
      }
      // number selection within the current page
      if ((k >= '0') && (k <= '9')) {
        int keyNum = k - '0';
        // only allow numbers in visible range
        int minIdx = (logListPage == 0) ? 0 : 5;
        int maxIdx = (logListPage == 0) ? 4 : 9;
        if (keyNum >= minIdx && keyNum <= maxIdx) {
          int slot = userIndexToSlot(keyNum);
          if (slot >= 0) {
            Serial.print("[OPEN SLOT "); Serial.print(keyNum);
            Serial.print("] sender="); Serial.print(logBuf[slot].sender);
            Serial.print(" nonce=");  Serial.println(logBuf[slot].nonceHex);
            drawLogViewScreen(slot);
          }
        }
      }
      return;

    case SCREEN_LOG_VIEW:
      if (k == 'D') { 
        // return to list, keep same page
        drawLogListScreen(); 
      }
      return;

    case SCREEN_HOME:
    default:
      break;
  }

  // SCREEN_HOME keys
  switch (k) {
    case '1':
    case '2':
    case '3': {
      uint8_t which = static_cast<uint8_t>(k - '0');
      String msg = buildMessage(which);
      loraSend(msg);
      lastRx = "TX: " + msg;
      drawHome();
      break;
    }
    case 'C':
      logListPage = 0;  // start at 0–4 page
      drawLogListScreen();
      break;
    default:
      break;
  }
}

// ---------- RX handler ----------
void handleLoRaRx() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String incoming = "";
    while (LoRa.available()) incoming += (char)LoRa.read();

    String shown = incoming; // default (plaintext)
    bool handledEncrypted = false;

    if(incoming.startsWith("ENC|")){
      String tag, nHex, cHex, tHex;
      if(parseEncryptedFrame(incoming, tag, nHex, cHex, tHex)){
        // Show nonce (hex) on RX
        Serial.print("[RX NONCE] ");
        Serial.println(nHex);

        // Try each RX key
        for(size_t i=0;i<RX_KEYS_COUNT;i++){
          String plain;
          if(tryDecryptWithKey(rxKeys[i], tag, nHex, cHex, tHex, plain)){
            shown = plain; handledEncrypted = true;

            // Persist to NVS log (only authenticated)
            addLogEntryPersistent(tag, plain, nHex);

            Serial.printf("[RX] (AUTH) key=%s RSSI=%d SNR=%.1f\n",
                          rxKeys[i].label, LoRa.packetRssi(), LoRa.packetSnr());
            break;
          }
        }
        if(!handledEncrypted){
          shown = "[AUTH FAIL] Unable to decrypt";
          Serial.printf("[RX] (AUTH FAIL) RSSI=%d SNR=%.1f\n",
                        LoRa.packetRssi(), LoRa.packetSnr());
        }
      }else{
        shown = "[PARSE FAIL] Bad ENC frame";
      }
    }else{
      Serial.printf("[RX] (PLAINTEXT) RSSI=%d SNR=%.1f\n",
                    LoRa.packetRssi(), LoRa.packetSnr());
    }

    lastRx = shown;
    rxFull = shown;
    screen = SCREEN_RX;
    drawRxScreen();
  }
}

// ---------- GPS periodic save ----------
void maybeDoFirstSave() {
  if (firstSaveDone) return;
  if (haveLiveFix()) {
    saveNVS(curLat, curLng);
    nvsLat = curLat; nvsLng = curLng;
    firstSaveDone = true;
    lastSave = millis(); // start the 5-min cadence from now
    Serial.println("[NVS] First GPS saved");
  }
}

void maybeSaveFix() {
  if (!firstSaveDone) return;
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

  loadNVS();              // GPS lat/lng
  loadMsgLogFromNVS();    // last-10 message log

  // Do NOT mark firstSaveDone from existing NVS; wait for fresh fix first
  firstSaveDone = false;

  // ---- CRYPTO KEYS INIT ----
  deriveKey(TX_KEY_LABEL, txKey.key);
  txKey.label = TX_KEY_LABEL;
  for(size_t i=0;i<RX_KEYS_COUNT;i++){
    deriveKey(RX_KEYS_LABELS[i], rxKeys[i].key);
    rxKeys[i].label = RX_KEYS_LABELS[i];
  }
  Serial.print("[KEY] TX label: "); Serial.println(txKey.label);
  for(size_t i=0;i<RX_KEYS_COUNT;i++){
    Serial.print("[KEY] RX label: "); Serial.println(rxKeys[i].label);
  }

  drawHome();
}

// ---------- Loop ----------
void loop() {
  handleGPSStream();   // continuously parse GPS data
  maybeDoFirstSave();  // wait for first valid fix then save once
  handleKeypad();      // 'C' list (paged), '#' next page, numbers select, 'D' back
  handleLoRaRx();      // show received text (RX screen)
  maybeSaveFix();      // periodic save only after firstSaveDone
}
