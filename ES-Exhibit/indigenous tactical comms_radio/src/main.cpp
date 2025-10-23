#include "app_config.h"
#include "display.hpp"
#include "gps.hpp"
#include "crypto.hpp"
#include "storage.hpp"
#include "lora_radio.hpp"
#include "ui.hpp"
#include "keypad.hpp"

void setup() {
    Serial.begin(115200);
    delay(100);

    // Initialize display and show boot message
    initDisplay();
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 0);
    display.println("Booting...");
    display.display();
    // If display init failed, we continue without visible output

    // Initialize GPS serial port
    GPSSerial.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);

    // Initialize LoRa radio
    SPI.begin(18, 19, 23);
    LoRa.setPins(PIN_LORA_SS, PIN_LORA_RST, PIN_LORA_DIO0);
    if (!LoRa.begin(LORA_FREQ)) {
        display.clearDisplay();
        display.setCursor(0, 0);
        display.println("LoRa init failed!");
        display.display();
    }
    LoRa.setSpreadingFactor(12);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    LoRa.enableCrc();
    LoRa.setSyncWord(0x34);
    LoRa.disableInvertIQ();

    // Load stored GPS coordinates and message log from NVS
    loadNVS();
    loadMsgLogFromNVS();

    // Derive cryptographic keys for TX and RX
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

    // Create transmission queue and TX task
    gTxQ = xQueueCreate(3, sizeof(TxItem));
    if (!gTxQ) {
        Serial.println("[TXQ] Failed to create queue!");
    } else {
        BaseType_t ok = xTaskCreatePinnedToCore(txTask, "txTask", 12288, nullptr, 1, &gTxTask, 1);
        if (ok != pdPASS) {
            Serial.println("[TXQ] Failed to create txTask!");
        }
    }

    // Create receive queue and RX task
    gRxQ = xQueueCreate(RX_QUEUE_LEN, sizeof(RxItem));
    if (!gRxQ) {
        Serial.println("[RXQ] Failed to create queue!");
    } else {
        BaseType_t ok = xTaskCreatePinnedToCore(rxTask, "rxTask", 12288, nullptr, 1, &gRxTask, 1);
        if (ok != pdPASS) {
            Serial.println("[RXQ] Failed to create rxTask!");
        }
    }

    // Set LoRa receive ISR and enter receive mode
    LoRa.onReceive(rxIsr);
    LoRa.receive();

    // Show the initial home screen
    drawHome();
}

void loop() {
    // Handle incoming GPS data and periodic saving of last fix
    handleGPSStream();
    maybePersistFix();

    // Handle keypad input
    handleKeypad();

    // If in compose mode, handle timeouts and UI updates
    if (screen == SCREEN_COMPOSE) {
        Compose::autoCommitIfTimeout();
        Compose::render();
    }

    // Yield to other tasks
    delay(1);
}
