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

// --- FreeRTOS ---
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// --- WiFi / HTTP (ESP32) ---
#include <WiFi.h>
#include <HTTPClient.h>

// ===================== DASHBOARD / WIFI CONFIG =====================
static const char* WIFI_SSID     = "Ssid";
static const char* WIFI_PASS     = "password!";
static const char* DASH_POST_URL = "http://192.168.68.131:3000/api/v1/messages";
static const char* DEVICE_API_TOKEN = "dev-secret-123";  // change me

// Non-blocking-ish WiFi connector; returns true if connected
bool ensureWifi(uint32_t timeout_ms = 8000) {
  if (WiFi.status() == WL_CONNECTED) return true;

  if (WiFi.getMode() == WIFI_MODE_NULL) {
    WiFi.mode(WIFI_STA);
  }
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.begin(WIFI_SSID, WIFI_PASS);
  }

  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
    delay(50); // let RTOS breathe
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[NET] WiFi OK, IP="); Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("[NET] WiFi connect timeout");
  return false;
}

// Posts JSON {sender_tag, message, nonce}; returns HTTP code or <0 on failure
int sendToDashboard(const String& senderTag, const String& message, const String& nonceHex) {
  if (!ensureWifi()) return -1;

  HTTPClient http;
  http.begin(DASH_POST_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + DEVICE_API_TOKEN);

  // Build minimal JSON (escape quotes/newlines if you expect them)
  String payload;
  payload.reserve(senderTag.length() + message.length() + nonceHex.length() + 64);
  payload += "{\"sender_tag\":\""; payload += senderTag;  // NOTE: for production, escape JSON
  payload += "\",\"message\":\"";  payload += message;
  payload += "\",\"nonce\":\"";    payload += nonceHex;
  payload += "\"}";

  int code = http.POST(payload);
  if (code > 0) {
    String resp = http.getString();
    Serial.printf("[HTTP] %d %s\n", code, resp.c_str());
  } else {
    Serial.printf("[HTTP] POST failed: %d\n", code);
  }
  http.end();
  return code;
}
// ===================================================================

// ---------- PIN MAP ----------
static const int PIN_LORA_SS   = 5;
static const int PIN_LORA_RST  = 14;
static const int PIN_LORA_DIO0 = 26;         // used for Rx ISR
static const long LORA_FREQ    = LORA_BAND;  // 433E6 from build_flags

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

enum ScreenState { SCREEN_HOME = 0, SCREEN_RX = 1, SCREEN_LOG_LIST = 2, SCREEN_LOG_VIEW = 3, SCREEN_COMPOSE = 4 };
ScreenState screen = SCREEN_HOME;

double curLat = NAN, curLng = NAN;
uint32_t curSat = 0;
double curHdop = NAN;
uint32_t lastFixMillis = 0;

double nvsLat = NAN, nvsLng = NAN;

const uint32_t SAVE_INTERVAL = 5UL * 60UL * 1000UL;
uint32_t lastSave = 0;

String lastRx = "";
String rxFull  = "";

// --------- CRYPTO CONSTANTS / HELPERS ---------
static const char* TX_SENDER_TAG = "HQ";
static const char* TX_KEY_LABEL  = "HQ-PH";
static const char* RX_KEYS_LABELS[] = {"unit-001", "unit-002"};
static const size_t RX_KEYS_COUNT = sizeof(RX_KEYS_LABELS)/sizeof(RX_KEYS_LABELS[0]);

static const size_t KEY_LEN   = 32;  // 256-bit
static const size_t NONCE_LEN = 12;  // 96-bit nonce
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

const int LOG_CAP = 10;
MsgEntry logBuf[LOG_CAP];
uint8_t logCount = 0;
uint8_t logHead  = 0;
uint8_t logListPage = 0;

// --------- FORWARD DECLARATIONS ---------
void drawHome();
void drawLogListScreen();
void drawLogViewScreen(int selectedSlot);

bool tryDecryptWithKey(const KeyEntry& k, const String& senderTag,
                       const String& nonceHex, const String& cHex,
                       const String& tHex, String& outPlain);

bool parseEncryptedFrame(const String& s, String& senderTag, String& nHex, String& cHex, String& tHex);

// ---------------- NVS helpers for the message log ----------------
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

void addLogEntryPersistent(const String& sender, const String& message, const String& nonceHex) {
  logBuf[logHead].sender   = sender;
  logBuf[logHead].msg      = message;
  logBuf[logHead].nonceHex = nonceHex;

  saveMsgLogSlotToNVS(logHead);

  logHead = (logHead + 1) % LOG_CAP;
  if (logCount < LOG_CAP) logCount++;
  saveMsgLogMetaToNVS();
}

int userIndexToSlot(int userIdx) {
  if (userIdx < 0 || userIdx >= logCount) return -1;
  int newestSlot = (int)logHead - 1;
  if (newestSlot < 0) newestSlot += LOG_CAP;
  int slot = newestSlot - userIdx;
  while (slot < 0) slot += LOG_CAP;
  return slot % LOG_CAP;
}

// --------- UI Helpers ---------
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

// --- HOME SCREEN ---
void drawHome() {
  screen = SCREEN_HOME;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);
  display.println(TX_SENDER_TAG);

  display.setCursor(0, 12);
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
  display.println("4: Custom message");
  display.println("C: View saved msgs");

  display.display();
}

// --- LOG LIST ---
void drawLogListScreen() {
  screen = SCREEN_LOG_LIST;
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.println("SELECT MSG 0-9");

  int startIdx = (logListPage == 0) ? 0 : 5;
  int endIdx   = startIdx + 5; // exclusive

  for (int i = startIdx; i < endIdx; ++i) {
    int slot = userIndexToSlot(i);
    String label = String(i) + String(". ");
    if (slot >= 0) label += truncate21(logBuf[slot].sender);
    else           label += "(empty)";
    int line = i - startIdx;
    display.setCursor(0, 12 + line * 10);
    display.println(label);
  }

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

// --- GPS stream handler (no raw NMEA printing) ---
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
  for(size_t i=0;i<len;i++){ s += nybble((data[i]>>4)&0x0F); s += nybble((data[i])&0x0F); }
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
bool buildEncryptedFrame(const String& plaintext, String& outFrame) {
  uint8_t nonce[NONCE_LEN];
  makeNonce(nonce);

  String nHex = toHex(nonce, NONCE_LEN);
  Serial.print("[TX NONCE] ");
  Serial.println(nHex);

  const size_t plen = plaintext.length();
  std::unique_ptr<uint8_t[]> pbuf(new uint8_t[plen]);
  memcpy(pbuf.get(), plaintext.c_str(), plen);

  std::unique_ptr<uint8_t[]> cbuf(new uint8_t[plen]);
  uint8_t tag[TAG_LEN];

  const uint8_t* aad = (const uint8_t*)TX_SENDER_TAG;
  size_t aad_len = strlen(TX_SENDER_TAG);

  bool ok = aesGcmEncrypt(txKey.key, pbuf.get(), plen,
                          nonce, NONCE_LEN,
                          aad, aad_len,
                          cbuf.get(), tag, TAG_LEN);
  if(!ok){
    Serial.println("[ENC] AES-GCM encrypt failed");
    return false;
  }

  outFrame = "ENC|tag="; outFrame += TX_SENDER_TAG;
  outFrame += "|n="; outFrame += nHex;
  outFrame += "|c="; outFrame += toHex(cbuf.get(), plen);
  outFrame += "|t="; outFrame += toHex(tag, TAG_LEN);
  return true;
}

//
// ===================== TX FreeRTOS QUEUE & TASK =====================
#define TX_MSG_MAX 256

struct TxItem {
  char text[TX_MSG_MAX];
};

static QueueHandle_t gTxQ = nullptr;
static TaskHandle_t  gTxTask = nullptr;

bool loraSendEncrypted_Blocking(const String& plaintext){
  String frame;
  if(!buildEncryptedFrame(plaintext, frame)) return false;
  Serial.print("[TX] "); Serial.println(frame);
  LoRa.beginPacket();
  LoRa.print(frame);
  LoRa.endPacket();     // BLOCKING; returns when TX done
  LoRa.receive();       // ensure RX mode resumes
  return true;
}

void loraSend(const String& s) {
  if(loraSendEncrypted_Blocking(s)){
    Serial.print("[TX HUMAN] "); Serial.println(s);
  }else{
    Serial.println("[WARN] Fallback to plaintext TX (blocking)");
    LoRa.beginPacket();
    LoRa.print(s);
    LoRa.endPacket();
    LoRa.receive();
  }
}

bool enqueueTx(const String& msg) {
  if (!gTxQ) return false;
  TxItem item;
  size_t n = msg.length();
  if (n >= TX_MSG_MAX) n = TX_MSG_MAX - 1; // truncate
  memcpy(item.text, msg.c_str(), n);
  item.text[n] = '\0';
  if (xQueueSend(gTxQ, &item, 0) != pdPASS) {
    Serial.println("[TXQ] Full (3). Message dropped.");
    return false;
  }
  Serial.printf("[TXQ] Enqueued: %u bytes\n", (unsigned)n);
  return true;
}

void txTask(void* pv) {
  Serial.printf("[TXTASK] started on core %d, prio %u\n", xPortGetCoreID(), uxTaskPriorityGet(nullptr));
  TxItem item;
  for (;;) {
    if (xQueueReceive(gTxQ, &item, portMAX_DELAY) == pdTRUE) {
      String msg = String(item.text);
      Serial.print("[TXQ] Dequeue -> TX: "); Serial.println(msg);
      loraSend(msg); // blocking send
      vTaskDelay(pdMS_TO_TICKS(1));
    }
  }
}
// ====================================================================

//
// ===================== RX FreeRTOS QUEUE & TASK =====================
#define RX_MAX_BYTES 260
#define RX_QUEUE_LEN 8

struct RxItem {
  uint16_t len;
  int rssi;
  float snr;
  char data[RX_MAX_BYTES];
};

static QueueHandle_t gRxQ = nullptr;
static TaskHandle_t  gRxTask = nullptr;

// ISR-safe receive callback set via LoRa.onReceive(...)
void rxIsr(int packetSize) {
  if (packetSize <= 0) return;

  RxItem item;
  item.len = 0;
  while (LoRa.available() && item.len < (RX_MAX_BYTES - 1)) {
    item.data[item.len++] = (char)LoRa.read();
  }
  item.data[item.len] = '\0';
  item.rssi = LoRa.packetRssi();
  item.snr  = LoRa.packetSnr();

  BaseType_t hpw = pdFALSE;
  xQueueSendFromISR(gRxQ, &item, &hpw);
  if (hpw == pdTRUE) {
    portYIELD_FROM_ISR();
  }
}

// Slow path: decrypt, log, UI, and HTTP POST
void rxTask(void* pv) {
  Serial.printf("[RXTASK] started on core %d, prio %u\n", xPortGetCoreID(), uxTaskPriorityGet(nullptr));
  RxItem item;
  for (;;) {
    if (xQueueReceive(gRxQ, &item, portMAX_DELAY) == pdTRUE) {
      String incoming(item.data);

      String shown = incoming; // default (plaintext)
      bool handledEncrypted = false;
      String tag, nHex, cHex, tHex;

      if(incoming.startsWith("ENC|")){
        if(parseEncryptedFrame(incoming, tag, nHex, cHex, tHex)){
          for(size_t i=0;i<RX_KEYS_COUNT;i++){
            String plain;
            if(tryDecryptWithKey(rxKeys[i], tag, nHex, cHex, tHex, plain)){
              shown = plain; handledEncrypted = true;
              addLogEntryPersistent(tag, plain, nHex);
              Serial.printf("[RX] (AUTH) key=%s RSSI=%d SNR=%.1f\n",
                            rxKeys[i].label, item.rssi, item.snr);

              // >>>>>>>>>>>>> HTTP POST (only on successful decrypt) <<<<<<<<<<<<<
              int rc = sendToDashboard(tag, plain, nHex);
              if (rc <= 0) {
                Serial.println("[HTTP] send failed (skipped or offline)");
              }
              // >>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>

              break;
            }
          }
          if(!handledEncrypted){
            shown = "[AUTH FAIL] Unable to decrypt";
            Serial.printf("[RX] (AUTH FAIL) RSSI=%d SNR=%.1f\n",
                          item.rssi, item.snr);
          }
        }else{
          shown = "[PARSE FAIL] Bad ENC frame";
        }
      }else{
        Serial.printf("[RX] (PLAINTEXT) RSSI=%d SNR=%.1f\n",
                      item.rssi, item.snr);
        // If you want to POST plaintext too, uncomment:
        // int rc = sendToDashboard("PLAINTEXT", shown, "");
      }

      lastRx = shown;
      rxFull = shown;
      screen = SCREEN_RX;
      drawRxScreen();
    }
  }
}
// ====================================================================


// === COMPOSE (enqueueTx on send) ====================================
namespace Compose {
  static const unsigned long TAP_TIMEOUT = 700;   // ms before auto-commit
  static const size_t MAX_MSG_LEN = 512;

  static const char* MT_MAP[10] = {
    " ", ".,?!1", "ABC2","DEF3","GHI4","JKL5","MNO6","PQRS7","TUV8","WXYZ9"
  };

  static String message = "";
  static char   activeKey = 0;
  static uint8_t activeIdx = 0;
  static unsigned long lastTap = 0;
  static bool dirty = true;

  inline bool isDigitKey(char k){ return (k >= '0' && k <= '9'); }

  void commitActiveIfAny() {
    if (activeKey == 0) return;
    uint8_t group = activeKey - '0';
    char ch = MT_MAP[group][activeIdx];
    if (message.length() < MAX_MSG_LEN) message += ch;
    activeKey = 0; activeIdx = 0;
    dirty = true;
  }

  void backspace() {
    if (activeKey != 0) { activeKey = 0; activeIdx = 0; dirty = true; return; }
    if (message.length() > 0) { message.remove(message.length() - 1); dirty = true; }
  }

  void newline() {
    commitActiveIfAny();
    if (message.length() < MAX_MSG_LEN) { message += '\n'; dirty = true; }
  }

  void drawWrapped(const String& s, int x, int y, uint8_t charsPerLine, uint8_t maxLines) {
    uint16_t start = 0; uint8_t line = 0;
    while (start < s.length() && line < maxLines) {
      int nl = s.indexOf('\n', start);
      uint16_t len = min<uint16_t>(charsPerLine, s.length() - start);
      if (nl >= 0 && (uint16_t)(nl - start) < len) len = nl - start;
      display.setCursor(x, y + line * 8);
      display.print(s.substring(start, start + len));
      start += len;
      if (nl >= 0 && (uint16_t)(nl - (start - len)) == 0) start++;
      line++;
    }
  }

  String buildPreview() {
    if (activeKey == 0) return message;
    uint8_t group = activeKey - '0';
    char ch = MT_MAP[group][activeIdx];
    String s = message;
    if (s.length() < MAX_MSG_LEN) s += ch;
    return s;
  }

  void render() {
    if (!dirty) return;
    dirty = false;

    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("Custom Msg:");

    String preview = buildPreview();
    if (activeKey != 0 && preview.length() < MAX_MSG_LEN) preview += '_';

    drawWrapped(preview, 0, 10, 21, 6);

    display.setCursor(0, 56);
    display.print("*=Bksp  #=Newline  A=Send  D=Back");
    display.display();
  }

  void handleMultiTap(char k) {
    if (k == '0') { commitActiveIfAny(); if (message.length() < MAX_MSG_LEN) message += ' '; dirty = true; return; }
    uint8_t group = k - '0';
    const char* seq = MT_MAP[group];
    uint8_t seqLen = strlen(seq);
    unsigned long now = millis();

    if (activeKey == k && (now - lastTap) <= TAP_TIMEOUT) {
      activeIdx = (activeIdx + 1) % seqLen;  dirty = true;
    } else {
      commitActiveIfAny();
      activeKey = k; activeIdx = 0; dirty = true;
    }
    lastTap = now;
  }

  void handleKey(char k) {
    if (!k) return;

    if (!isDigitKey(k) && k != '*' && k != '#'
        && k != 'A' && k != 'D') {
      commitActiveIfAny();
    }

    switch (k) {
      case '*': backspace(); break;
      case '#': newline();   break;
      case 'A': {
        commitActiveIfAny();
        if (message.length() > 0) {
          if (!enqueueTx(message)) {
            Serial.println("[TXQ] Send refused: queue full");
          }
        }
        message = ""; activeKey = 0; activeIdx = 0; dirty = true;
        screen = SCREEN_HOME; drawHome();
        break;
      }
      case 'D':
        activeKey = 0; activeIdx = 0; screen = SCREEN_HOME; drawHome(); break;
      default:
        if (isDigitKey(k)) handleMultiTap(k);
        break;
    }
  }

  void autoCommitIfTimeout() {
    if (activeKey == 0) return;
    if (millis() - lastTap > TAP_TIMEOUT) commitActiveIfAny();
  }

  void enter() { screen = SCREEN_COMPOSE; dirty = true; render(); }
} // namespace Compose
// ===================================================================

void handleKeypad() {
  char k = keypad.getKey();
  if (!k) return;

  if (screen == SCREEN_COMPOSE) { Compose::handleKey(k); return; }

  switch (screen) {
    case SCREEN_RX:
      if (k == 'D') drawHome();
      return;

    case SCREEN_LOG_LIST:
      if (k == 'D') { drawHome(); return; }
      if (k == '#') { logListPage ^= 1; drawLogListScreen(); return; }
      if ((k >= '0') && (k <= '9')) {
        int keyNum = k - '0';
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
      if (k == 'D') { drawLogListScreen(); }
      return;

    case SCREEN_HOME:
    default:
      break;
  }

  switch (k) {
    case '1':
    case '2':
    case '3': {
      uint8_t which = static_cast<uint8_t>(k - '0');
      String msg = buildMessage(which);
      if (!enqueueTx(msg)) {
        Serial.println("[TXQ] Send refused: queue full");
      } else {
        lastRx = "TX: " + msg;
      }
      drawHome();
      break;
    }
    case '4':
      Compose::enter();
      break;
    case 'C':
      logListPage = 0; drawLogListScreen(); break;
    default:
      break;
  }
}

// ---------- GPS periodic save (trimmed per your request) ----------
void maybePersistFix() {
  if (!haveLiveFix()) {
    return;
  }
  if (lastSave == 0 || (millis() - lastSave) >= SAVE_INTERVAL) {
    bool bootSave = (lastSave == 0);
    (void)bootSave;
    saveNVS(curLat, curLng);
    nvsLat = curLat;
    nvsLng = curLng;
    lastSave = millis();
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

  // WiFi early, just in case you want to be online before first RX
  ensureWifi();

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
  LoRa.enableCrc();
  LoRa.setSyncWord(0x34);
  LoRa.disableInvertIQ();

  loadNVS();
  loadMsgLogFromNVS();

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

  // ==== Create TX queue (len=3) and TX task (prio=1) ====
  gTxQ = xQueueCreate(3, sizeof(TxItem));
  if (!gTxQ) {
    Serial.println("[TXQ] Failed to create queue!");
  } else {
    BaseType_t ok = xTaskCreatePinnedToCore(
      txTask, "txTask", 12288, nullptr,
      1, // priority = 1
      &gTxTask, 1
    );
    if (ok != pdPASS) Serial.println("[TXQ] Failed to create txTask!");
  }

  // ==== Create RX queue (len=8) and RX task (prio=1) ====
  gRxQ = xQueueCreate(RX_QUEUE_LEN, sizeof(RxItem));
  if (!gRxQ) {
    Serial.println("[RXQ] Failed to create queue!");
  } else {
    BaseType_t ok = xTaskCreatePinnedToCore(
      rxTask, "rxTask", 12288, nullptr,
      1, // priority = 1
      &gRxTask, 1
    );
    if (ok != pdPASS) Serial.println("[RXQ] Failed to create rxTask!");
  }

  // Hook DIO0 → ISR callback for RX, and enter RX mode
  LoRa.onReceive(rxIsr);
  LoRa.receive();

  drawHome();
}

// ---------- Loop ----------
void loop() {
  handleGPSStream();
  maybePersistFix();
  handleKeypad();

  if (screen == SCREEN_COMPOSE) {
    Compose::autoCommitIfTimeout();
    Compose::render();
  }

  delay(1); // let RTOS run tasks
}

// ---------- Helpers defined after forward decls ----------
bool tryDecryptWithKey(const KeyEntry& k, const String& senderTag,
                       const String& nonceHex, const String& cHex, const String& tHex,
                       String& outPlain){
  uint8_t nonce[NONCE_LEN];
  if(!fromHex(nonceHex, nonce, NONCE_LEN)) return false;

  const size_t clen = cHex.length()/2;
  std::unique_ptr<uint8_t[]> cbuf(new uint8_t[clen]);
  if(!fromHex(cHex, cbuf.get(), clen)) return false;

  uint8_t tag[TAG_LEN];
  if(!fromHex(tHex, tag, TAG_LEN)) return false;

  std::unique_ptr<uint8_t[]> pbuf(new uint8_t[clen]);

  const uint8_t* aad = (const uint8_t*)senderTag.c_str();
  size_t aad_len = senderTag.length();

  bool ok = aesGcmDecrypt(k.key, cbuf.get(), clen, nonce, NONCE_LEN, aad, aad_len, tag, TAG_LEN, pbuf.get());
  if(!ok) return false;

  outPlain = String((const char*)pbuf.get(), clen);
  return true;
}

bool parseEncryptedFrame(const String& s, String& senderTag, String& nHex, String& cHex, String& tHex){
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
