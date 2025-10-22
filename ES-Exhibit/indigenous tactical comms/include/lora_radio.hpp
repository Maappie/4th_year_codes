#pragma once
#include "app_config.h"
#include <Arduino.h>

// LoRa radio interface and encrypted send/receive
bool initLoRaRadio();
void loraSend(const String& s);
bool loraSendEncrypted(const String& plaintext);
void handleLoRaRx();
