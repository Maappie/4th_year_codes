#include <Arduino.h>
#include "app_config.h"
#include "display.hpp"
#include "gps.hpp"
#include "lora_radio.hpp"
#include "storage.hpp"
#include "crypto.hpp"
#include "keypad.hpp"
#include "ui.hpp"

void setup() {
    // ---------- Setup ----------
    Serial.begin(115200);
    delay(100);

    initDisplay();
    GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    initLoRaRadio();
    loadNVS();
    loadMsgLogFromNVS();

    // ---- CRYPTO KEYS INIT ----
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

    drawHome();
}

void loop() {
    // ---------- Loop ----------
    handleGPSStream();    // continuously parse GPS data
    printNMEADump1Hz();   // dump raw NMEA once per second
    maybePersistFix();    // boot save + periodic 5-min saves when valid
    handleKeypad();       // handle keypad input (UI navigation & compose)
    handleLoRaRx();       // process LoRa RX and update UI if message received

    if (screen == SCREEN_COMPOSE) {
        Compose::autoCommitIfTimeout();
        Compose::render();  // update compose screen if needed
    }
}
