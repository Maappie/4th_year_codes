#include "lora_radio.hpp"
#include "app_config.h"
#include <SPI.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "crypto.hpp"
#include "wifi.hpp"
#include "storage.hpp"
#include "ui.hpp"

// LoRa radio TX/RX queue and task
#define TX_MSG_MAX   256
#define RX_MAX_BYTES 260
#define RX_QUEUE_LEN 8

struct TxItem {
    char text[TX_MSG_MAX];
};

struct RxItem {
    uint16_t len;
    int rssi;
    float snr;
    char data[RX_MAX_BYTES];
};

static QueueHandle_t gTxQ = nullptr;
static TaskHandle_t  gTxTask = nullptr;
static QueueHandle_t gRxQ = nullptr;
static TaskHandle_t  gRxTask = nullptr;

// Initialize LoRa radio hardware
bool initLoRa() {
    // SPI pins are set via SPI.begin (with defaults)
    LoRa.setPins(PIN_LORA_SS, PIN_LORA_RST, PIN_LORA_DIO0);
    bool ok = LoRa.begin(LORA_FREQ);
    if (!ok) {
        // If LoRa init fails, we still continue to set parameters
    }
    LoRa.setSpreadingFactor(12);
    LoRa.setSignalBandwidth(125E3);
    LoRa.setCodingRate4(5);
    LoRa.enableCrc();
    LoRa.setSyncWord(0x34);
    LoRa.disableInvertIQ();
    return ok;
}

// Blocking send of an encrypted LoRa message
static bool loraSendEncrypted_Blocking(const String& plaintext) {
    String frame;
    if (!buildEncryptedFrame(plaintext, frame)) {
        return false;
    }
    Serial.print("[TX] ");
    Serial.println(frame);
    LoRa.beginPacket();
    LoRa.print(frame);
    LoRa.endPacket();   // blocking until TX done
    LoRa.receive();     // resume RX mode
    return true;
}

// LoRa send (with encryption, fallback to plaintext)
static void loraSend(const String& s) {
    if (loraSendEncrypted_Blocking(s)) {
        Serial.print("[TX HUMAN] ");
        Serial.println(s);
    } else {
        Serial.println("[WARN] Fallback to plaintext TX (blocking)");
        LoRa.beginPacket();
        LoRa.print(s);
        LoRa.endPacket();
        LoRa.receive();
    }
}

// Enqueue a message for later TX (non-blocking)
bool enqueueTx(const String& msg) {
    if (!gTxQ) {
        return false;
    }
    TxItem item;
    size_t n = msg.length();
    if (n >= TX_MSG_MAX) {
        n = TX_MSG_MAX - 1;
    }
    memcpy(item.text, msg.c_str(), n);
    item.text[n] = '\0';
    if (xQueueSend(gTxQ, &item, 0) != pdPASS) {
        Serial.println("[TXQ] Full (3). Message dropped.");
        return false;
    }
    Serial.printf("[TXQ] Enqueued: %u bytes\n", (unsigned)n);
    return true;
}

// TX task: sends messages from queue
static void txTask(void* pv) {
    Serial.printf("[TXTASK] started on core %d, prio %u\n", xPortGetCoreID(), uxTaskPriorityGet(nullptr));
    TxItem item;
    for (;;) {
        if (xQueueReceive(gTxQ, &item, portMAX_DELAY) == pdTRUE) {
            String msg = String(item.text);
            Serial.print("[TXQ] Dequeue -> TX: ");
            Serial.println(msg);
            loraSend(msg);  // blocking send
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

// ISR for LoRa receive (called from DIO0 interrupt)
static void rxIsr(int packetSize) {
    if (packetSize <= 0) {
        return;
    }
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

// RX task: handles incoming LoRa messages (decrypt, log, UI, HTTP)
static void rxTask(void* pv) {
    Serial.printf("[RXTASK] started on core %d, prio %u\n", xPortGetCoreID(), uxTaskPriorityGet(nullptr));
    RxItem item;
    for (;;) {
        if (xQueueReceive(gRxQ, &item, portMAX_DELAY) == pdTRUE) {
            String incoming(item.data);
            String shown = incoming;
            bool handledEncrypted = false;
            String tag, nHex, cHex, tHex;
            if (incoming.startsWith("ENC|")) {
                if (parseEncryptedFrame(incoming, tag, nHex, cHex, tHex)) {
                    for (size_t i = 0; i < RX_KEYS_COUNT; ++i) {
                        String plain;
                        if (tryDecryptWithKey(rxKeys[i], tag, nHex, cHex, tHex, plain)) {
                            shown = plain;
                            handledEncrypted = true;
                            addLogEntryPersistent(tag, plain, nHex);
                            Serial.printf("[RX] (AUTH) key=%s RSSI=%d SNR=%.1f\n", rxKeys[i].label, item.rssi, item.snr);
                            // HTTP POST on successful decrypt
                            int rc = sendToDashboard(tag, plain, nHex);
                            if (rc <= 0) {
                                Serial.println("[HTTP] send failed (skipped or offline)");
                            }
                            break;
                        }
                    }
                    if (!handledEncrypted) {
                        shown = "[AUTH FAIL] Unable to decrypt";
                        Serial.printf("[RX] (AUTH FAIL) RSSI=%d SNR=%.1f\n", item.rssi, item.snr);
                    }
                } else {
                    shown = "[PARSE FAIL] Bad ENC frame";
                }
            } else {
                Serial.printf("[RX] (PLAINTEXT) RSSI=%d SNR=%.1f\n", item.rssi, item.snr);
                // If needed, plaintext messages could also be forwarded via HTTP
                // int rc = sendToDashboard("PLAINTEXT", shown, "");
            }
            lastRx = shown;
            rxFull = shown;
            screen = SCREEN_RX;
            drawRxScreen();
        }
    }
}

// Start LoRa tasks and set up interrupt for RX
void startLoRaTasks() {
    // Create TX queue and task
    gTxQ = xQueueCreate(3, sizeof(TxItem));
    if (!gTxQ) {
        Serial.println("[TXQ] Failed to create queue!");
    } else {
        BaseType_t ok = xTaskCreatePinnedToCore(txTask, "txTask", 12288, nullptr, 1, &gTxTask, 1);
        if (ok != pdPASS) {
            Serial.println("[TXQ] Failed to create txTask!");
        }
    }
    // Create RX queue and task
    gRxQ = xQueueCreate(RX_QUEUE_LEN, sizeof(RxItem));
    if (!gRxQ) {
        Serial.println("[RXQ] Failed to create queue!");
    } else {
        BaseType_t ok = xTaskCreatePinnedToCore(rxTask, "rxTask", 12288, nullptr, 1, &gRxTask, 1);
        if (ok != pdPASS) {
            Serial.println("[RXQ] Failed to create rxTask!");
        }
    }
    // Set LoRa receive callback and enter receive mode
    LoRa.onReceive(rxIsr);
    LoRa.receive();
}
