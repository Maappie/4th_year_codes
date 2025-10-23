#pragma once
#include <Arduino.h>
#include <LoRa.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Maximum sizes for LoRa communication
#define TX_MSG_MAX    256
#define RX_MAX_BYTES  260
#define RX_QUEUE_LEN  8

// Queue item structures
struct TxItem {
    char text[TX_MSG_MAX];
};
struct RxItem {
    uint16_t len;
    int rssi;
    float snr;
    char data[RX_MAX_BYTES];
};

// Global queue and task handles
extern QueueHandle_t gTxQ;
extern TaskHandle_t  gTxTask;
extern QueueHandle_t gRxQ;
extern TaskHandle_t  gRxTask;

// LoRa radio functions and callbacks
bool enqueueTx(const String& msg);
void txTask(void* pv);
void rxTask(void* pv);
void rxIsr(int packetSize);
