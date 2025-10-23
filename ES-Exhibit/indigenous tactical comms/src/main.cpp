#include <Arduino.h>
#include "app_config.h"
#include "display.hpp"
#include "wifi.hpp"
#include "gps.hpp"
#include "crypto.hpp"
#include "storage.hpp"
#include "lora_radio.hpp"
#include "ui.hpp"
#include "keypad.hpp"

// Save last GPS fix to NVS periodically
void maybePersistFix() {
    if (!haveLiveFix()) {
        return;
    }
    static uint32_t lastSave = 0;
    if (lastSave == 0 || (millis() - lastSave) >= SAVE_INTERVAL) {
        bool bootSave = (lastSave == 0);
        (void)bootSave;
        saveNVS(curLat, curLng);
        nvsLat = curLat;
        nvsLng = curLng;
        lastSave = millis();
    }
}

void setup() {
    Serial.begin(115200);
    delay(100);
    if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
        // OLED init failed, continue headless
    }
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Booting...");
    display.display();
    // Connect WiFi (non-blocking)
    ensureWifi();
    // Initialize GPS serial port
    GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    // Initialize LoRa radio
    bool loraOk = initLoRa();
    if (!loraOk) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("LoRa init failed!");
        display.display();
    }
    // Load stored GPS location and message log
    loadNVS();
    loadMsgLogFromNVS();
    // Derive encryption keys
    deriveKey(TX_KEY_LABEL, txKey.key);
    txKey.label = TX_KEY_LABEL;
    for (size_t i = 0; i < RX_KEYS_COUNT; ++i) {
        deriveKey(RX_KEYS_LABELS[i], rxKeys[i].key);
        rxKeys[i].label = RX_KEYS_LABELS[i];
    }
    Serial.print("[KEY] TX label: ");
    Serial.println(txKey.label);
    for (size_t i = 0; i < RX_KEYS_COUNT; ++i) {
        Serial.print("[KEY] RX label: ");
        Serial.println(rxKeys[i].label);
    }
    // Start LoRa tasks and ISR
    startLoRaTasks();
    // Show the home screen
    drawHome();
}

void loop() {
    handleGPSStream();
    maybePersistFix();
    handleKeypad();
    if (screen == SCREEN_COMPOSE) {
        Compose::autoCommitIfTimeout();
        Compose::render();
    }
    delay(1);
}
