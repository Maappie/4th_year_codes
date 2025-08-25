#include <WiFi.h>
#include <HTTPClient.h>
#include <time.h>

const char* ssid       = ""; // use your own wifi ssid
const char* password   = ""; // use your own wifi pass
const char* serverName = ""; // use your own server ip

const int buttonPin = 25;                 // Button to GND, internal pull-up
constexpr uint32_t DEBOUNCE_MS = 300;      // fast debounce
int looped = 0;
// --------- Queue for press events ----------
QueueHandle_t pressQueue;                 // queue of bytes; value unused
volatile uint32_t lastIsrMs = 0;

// ---------- Time helpers ----------
String isoNow() {
  struct tm ti;
  if (!getLocalTime(&ti)) return "";      // if NTP not ready, you could fallback
  char buf[24];
  strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &ti);
  return String(buf);
}

// ---------- Networking (runs in background task) ----------
void sendPostOnce() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(serverName);
  http.addHeader("Content-Type", "application/json");

  String ts = isoNow();
  if (ts == "") { ts = "1970-01-01T00:00:00"; } // fallback if NTP not ready
  String body = "{\"device_id\":\"ESP32_REAL\",\"pressed_at\":\"" + ts + "\"}";

  int code = http.POST(body);
  Serial.print("POST -> ");
  Serial.println(code);
  http.end();
}

// Background task that drains the queue
void pressSenderTask(void* pv) {
  uint8_t token;
  for (;;) {
    if (xQueueReceive(pressQueue, &token, portMAX_DELAY) == pdTRUE) {
      sendPostOnce();
      // Optional small yield so we don't hog CPU between bursts
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
}

// ---------- ISR (very tiny) ----------
void IRAM_ATTR onButtonFall() {
  uint32_t now = millis();
  if (now - lastIsrMs >= DEBOUNCE_MS) {
    uint8_t token = 1;
    BaseType_t hpw = pdFALSE;
    xQueueSendFromISR(pressQueue, &token, &hpw);
    lastIsrMs = now;
    if (hpw) portYIELD_FROM_ISR();
  }
}

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  pinMode(buttonPin, INPUT_PULLUP);

  // WiFi
  WiFi.begin(ssid, password);
  Serial.print("WiFi...");
  while (WiFi.status() != WL_CONNECTED) { delay(200); Serial.print("."); }
  Serial.println(" connected");

  // NTP
  configTime(8 * 3600, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  struct tm ti;
  looped = 0;
  while (getLocalTime(&ti)) {
    Serial.print("."); 
    delay(1000);
    looped++;
    if (looped == 10){
      break;
    }
  }
  Serial.println("Done");

  // Queue + task
  pressQueue = xQueueCreate(32, sizeof(uint8_t));      // up to 32 queued presses
  xTaskCreatePinnedToCore(pressSenderTask, "pressSend", 4096, nullptr, 1, nullptr, APP_CPU_NUM);

  // Interrupt on falling edge (HIGH -> LOW)
  attachInterrupt(digitalPinToInterrupt(buttonPin), onButtonFall, FALLING);
}

void loop() {
  // Nothing blocking here. Device stays ultra responsive.
  // (Optional: WiFi reconnect logic if you want)
  vTaskDelay(pdMS_TO_TICKS(100));
}

