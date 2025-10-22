#pragma once
#include <Arduino.h>

void beginLoRa();
void loraSend(const String& s);
void handleLoRaRx();

// Exposed RX/TX buffers (used by UI/display)
extern String lastRx;
extern String rxFull;
