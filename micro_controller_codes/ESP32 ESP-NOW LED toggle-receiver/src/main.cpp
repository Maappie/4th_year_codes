#include <esp_now.h>
#include <WiFi.h>
#include <Arduino.h>
#include "esp_wifi.h"

const int LED_PIN = 2;
bool ledState = false;

// put your ESP8266 STA MAC here (replace)
uint8_t esp8266Address[] = {0x8C, 0xAA, 0xB5, 0x0C, 0x4A, 0xE8};

// helper to print MAC
void printMac(const uint8_t *mac) {
  for (int i = 0; i < 6; i++) {
    if (mac[i] < 0x10) Serial.print("0");
    Serial.print(mac[i], HEX);
    if (i < 5) Serial.print(":");
  }
  Serial.println();
}

void printHex(const uint8_t *data, int len) {
  for (int i = 0; i < len; i++) {
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    Serial.print(" ");
  }
  Serial.println();
}

// --- Callback when data is received ---
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.print("📩 Packet received, length = ");
  Serial.println(len);

  Serial.print("From MAC: ");
  printMac(mac);

  Serial.print("RAW hex: ");
  printHex(incomingData, len);

  String msg;
  for (int i = 0; i < len; i++) msg += (char)incomingData[i];

  Serial.print("Message: ");
  Serial.println(msg);

  if (msg == "TOGGLE") {
    ledState = !ledState;
    digitalWrite(LED_PIN, ledState);
    Serial.print("💡 LED is now ");
    Serial.println(ledState ? "ON" : "OFF");

    // Send ACK back
    const char *reply = "ACK";
    esp_now_send(esp8266Address, (uint8_t *)reply, strlen(reply));
    Serial.println("📤 Sent ACK to ESP8266");
  }
}

void setup() {
  Serial.begin(9600);
  pinMode(LED_PIN, OUTPUT);

  WiFi.disconnect();
  WiFi.mode(WIFI_STA);

  // Force channel 1 (same as ESP8266)
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  Serial.print("📡 ESP32 STA MAC: ");
  Serial.println(WiFi.macAddress());

  uint8_t ch; wifi_second_chan_t sc;
  esp_wifi_get_channel(&ch, &sc);
  Serial.print("📡 ESP32 channel (locked): ");
  Serial.println(ch);

  if (esp_now_init() != ESP_OK) {
    Serial.println("❌ ESP-NOW init failed!");
    return;
  }

  // add peer (so we can send ACK back)
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, esp8266Address, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  esp_now_register_recv_cb(OnDataRecv);

  Serial.println("📡 ESP32 ready, waiting for data...");
}

void loop() {
  Serial.println("I am esp32");
  delay(2000); // status print only; receives in callback
}
