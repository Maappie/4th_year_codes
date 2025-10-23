#include "wifi.hpp"

static const char* WIFI_SSID     = "Fake Extender";
static const char* WIFI_PASS     = "Aa1231325213!";
static const char* DASH_POST_URL = "http://192.168.68.131:3000/api/v1/messages";
static const char* DEVICE_API_TOKEN = "dev-secret-123";  // change me

// Non-blocking-ish WiFi connector; returns true if connected
bool ensureWifi(uint32_t timeout_ms) {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }
    if (WiFi.getMode() == WIFI_MODE_NULL) {
        WiFi.mode(WIFI_STA);
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        delay(50);  // let RTOS breathe
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("[NET] WiFi OK, IP=");
        Serial.println(WiFi.localIP());
        return true;
    }
    Serial.println("[NET] WiFi connect timeout");
    return false;
}

// Posts JSON {sender_tag, message, nonce}; returns HTTP code or <0 on failure
int sendToDashboard(const String& senderTag, const String& message, const String& nonceHex) {
    if (!ensureWifi()) {
        return -1;
    }
    HTTPClient http;
    http.begin(DASH_POST_URL);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + DEVICE_API_TOKEN);
    // Build minimal JSON (escape quotes/newlines if needed)
    String payload;
    payload.reserve(senderTag.length() + message.length() + nonceHex.length() + 64);
    payload += "{\"sender_tag\":\"";
    payload += senderTag;  // NOTE: for production, escape JSON
    payload += "\",\"message\":\"";
    payload += message;
    payload += "\",\"nonce\":\"";
    payload += nonceHex;
    payload += "\"}";
    int code = http.POST(payload);
    if (code > 0) {
        String resp = http.getString();
        Serial.printf("[HTTP] %d %s\n", code, resp.c_str());
    } else {
        Serial.printf("[HTTP] POST failed: %d\n", code);
    }
    http.end();
    return code;
}
