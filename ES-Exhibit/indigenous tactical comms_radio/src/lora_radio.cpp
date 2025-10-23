#include "lora_radio.hpp"
#include "crypto.hpp"    // for encryption and keys
#include "storage.hpp"   // for persistent logging
#include "ui.hpp"        // for UI updates on receive
#include <cstring>       // for memcpy

// Queue and task handles
QueueHandle_t gTxQ = nullptr;
TaskHandle_t  gTxTask = nullptr;
QueueHandle_t gRxQ = nullptr;
TaskHandle_t  gRxTask = nullptr;

// Send an encrypted message immediately (blocking until sent)
static bool loraSendEncrypted_Blocking(const String& plaintext) {
    String frame;
    if (!buildEncryptedFrame(plaintext, frame)) {
        return false;
    }
    Serial.print("[TX] ");
    Serial.println(frame);
    LoRa.beginPacket();
    LoRa.print(frame);
    LoRa.endPacket();    // blocking until TX done
    LoRa.receive();      // resume RX mode
    return true;
}

// Lower-level send: try encrypted, fallback to plaintext if encryption fails
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

// Enqueue a message for transmission (non-blocking)
bool enqueueTx(const String& msg) {
    if (!gTxQ) return false;
    TxItem item;
    size_t n = msg.length();
    if (n >= TX_MSG_MAX) n = TX_MSG_MAX - 1;
    memcpy(item.text, msg.c_str(), n);
    item.text[n] = '\0';
    if (xQueueSend(gTxQ, &item, 0) != pdPASS) {
        Serial.println("[TXQ] Full (3). Message dropped.");
        return false;
    }
    Serial.printf("[TXQ] Enqueued: %u bytes\n", (unsigned)n);
    return true;
}

// TX task: dequeues messages and sends via LoRa
void txTask(void* pv) {
    Serial.printf("[TXTASK] started on core %d, prio %u\n", xPortGetCoreID(), uxTaskPriorityGet(nullptr));
    TxItem item;
    for (;;) {
        if (xQueueReceive(gTxQ, &item, portMAX_DELAY) == pdTRUE) {
            String msg = String(item.text);
            Serial.print("[TXQ] Dequeue -> TX: ");
            Serial.println(msg);
            loraSend(msg);
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

// ISR callback for LoRa receive (called in interrupt context)
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
    BaseType_t higherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(gRxQ, &item, &higherPriorityTaskWoken);
    if (higherPriorityTaskWoken == pdTRUE) {
        portYIELD_FROM_ISR();
    }
}

// RX task: receives messages, decrypts if needed, logs, and updates UI
void rxTask(void* pv) {
    Serial.printf("[RXTASK] started on core %d, prio %u\n", xPortGetCoreID(), uxTaskPriorityGet(nullptr));
    RxItem item;
    for (;;) {
        if (xQueueReceive(gRxQ, &item, portMAX_DELAY) == pdTRUE) {
            String incoming(item.data);
            String shown = incoming;
            bool handledEncrypted = false;
            if (incoming.startsWith("ENC|")) {
                String tag, nHex, cHex, tHex;
                if (parseEncryptedFrame(incoming, tag, nHex, cHex, tHex)) {
                    for (size_t i = 0; i < RX_KEYS_COUNT; ++i) {
                        String plain;
                        if (tryDecryptWithKey(rxKeys[i], tag, nHex, cHex, tHex, plain)) {
                            shown = plain;
                            handledEncrypted = true;
                            addLogEntryPersistent(tag, plain, nHex);
                            Serial.printf("[RX] (AUTH) key=%s RSSI=%d SNR=%.1f\n",
                                          rxKeys[i].label, item.rssi, item.snr);
                            break;
                        }
                    }
                    if (!handledEncrypted) {
                        shown = "[AUTH FAIL] Unable to decrypt";
                        Serial.printf("[RX] (AUTH FAIL) RSSI=%d SNR=%.1f\n",
                                      item.rssi, item.snr);
                    }
                } else {
                    shown = "[PARSE FAIL] Bad ENC frame";
                }
            } else {
                Serial.printf("[RX] (PLAINTEXT) RSSI=%d SNR=%.1f\n", item.rssi, item.snr);
            }
            lastRx = shown;
            rxFull = shown;
            screen = SCREEN_RX;
            drawRxScreen();
        }
    }
}
