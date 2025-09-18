#include <Arduino.h>
#include <ESP8266WiFi.h>
extern "C" {
  #include <espnow.h>
}

uint8_t esp32Address[] = {0x44, 0x1D, 0x64, 0xF3, 0x35, 0xDC}; // REPLACE with your ESP32 STA MAC

#define BUTTON_PIN D2   // GPIO4 (active LOW with INPUT_PULLUP)

unsigned long lastIAmPrint = 0;
const unsigned long I_AM_INTERVAL = 3000;

const unsigned long debounceDelay = 50; // ms
unsigned long lastDebounceTime = 0;
int lastReading = HIGH;
int stableState = HIGH;

// --- Callbacks ---
void OnDataSent(uint8_t *mac_addr, uint8_t sendStatus) {
  Serial.print("📤 Send Status: ");
  Serial.println(sendStatus == 0 ? "Success" : "Fail");
}

void OnDataRecv(uint8_t *mac, uint8_t *incomingData, uint8_t len) {
  Serial.print("📩 Reply received: ");
  for (int i = 0; i < len; i++) Serial.print((char)incomingData[i]);
  Serial.println();
}

void setup() {
  Serial.begin(9600);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

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

  // Make sure ESP8266 can both send and receive
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);

  esp_now_register_send_cb(OnDataSent);
  esp_now_register_recv_cb(OnDataRecv);

  if (esp_now_add_peer(esp32Address, ESP_NOW_ROLE_COMBO, 1, NULL, 0) != 0) {
    Serial.println("❌ Failed to add peer");
    return;
  }
  Serial.println("✅ ESP32 peer added");
}

void sendToggle() {
  const char *msg = "TOGGLE";
  esp_now_send(esp32Address, (uint8_t *)msg, strlen(msg));
  Serial.println("📤 Sent: TOGGLE");
}

void loop() {
  unsigned long now = millis();

  // non-blocking "I am esp8266"
  if (now - lastIAmPrint >= I_AM_INTERVAL) {
    Serial.println("I am esp8266");
    lastIAmPrint = now;
  }

  // read button (fast, non-blocking debounce)
  int reading = digitalRead(BUTTON_PIN);
  if (reading != lastReading) {
    lastDebounceTime = now;
  }

  if ((now - lastDebounceTime) > debounceDelay) {
    if (reading != stableState) {
      stableState = reading;
      // stableState is the debounced value
      if (stableState == LOW) { // pressed (INPUT_PULLUP)
        sendToggle();
      }
    }
  }

  lastReading = reading;
  // no blocking delay here
}
