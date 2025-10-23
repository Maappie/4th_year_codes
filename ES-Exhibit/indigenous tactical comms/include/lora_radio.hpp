#ifndef LORA_RADIO_MODULE_H
#define LORA_RADIO_MODULE_H

#include <Arduino.h>
#include <LoRa.h>

// LoRa radio control and messaging
bool initLoRa();         // Initialize LoRa radio (returns false if init failed)
void startLoRaTasks();   // Start LoRa RX/TX tasks and enable receive mode

bool enqueueTx(const String& msg);  // Enqueue a message for transmission

#endif // LORA_RADIO_MODULE_H
