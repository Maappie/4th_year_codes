#ifndef WIFI_MODULE_H
#define WIFI_MODULE_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// WiFi and Dashboard communication
bool ensureWifi(uint32_t timeout_ms = 8000);
int sendToDashboard(const String& senderTag, const String& message, const String& nonceHex);

#endif // WIFI_MODULE_H
