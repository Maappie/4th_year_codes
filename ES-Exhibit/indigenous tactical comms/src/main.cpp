#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>

#include "app_config.h"
#include "display.hpp"
#include "gps.hpp"
#include "storage.hpp"
#include "lora_radio.hpp"
#include "crypto.hpp"
#include "ui.hpp"
#include "keypad.hpp"

void setup() {
  Serial.begin(115200);
  delay(100);

  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // headless if OLED fails
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("Booting...");
  display.display();

  GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

  beginLoRa();

  loadNVS();           // GPS lat/lng
  loadMsgLogFromNVS(); // message log

  // Keys
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

void loop() {
  handleGPSStream();
  printNMEADump1Hz();
  maybePersistFix();
  handleKeypad();
  handleLoRaRx();

  if (screen == SCREEN_COMPOSE) {
    Compose::autoCommitIfTimeout();
    Compose::render();
  }
}
