#include "gps.hpp"
#include "storage.hpp"

TinyGPSPlus gps;
HardwareSerial GPSSerial(2);
double curLat = NAN;
double curLng = NAN;
uint32_t curSat = 0;
double curHdop = NAN;
uint32_t lastFixMillis = 0;

bool haveLiveFix() {
    // A fix is considered live if valid and updated within last 5 seconds
    return gps.location.isValid() && gps.location.age() < 5000;
}

void updateLiveFix() {
    if (gps.location.isValid()) {
        curLat = gps.location.lat();
        curLng = gps.location.lng();
        curSat = gps.satellites.value();
        curHdop = gps.hdop.hdop();
        lastFixMillis = millis();
    }
}

// Handle incoming GPS NMEA stream (non-blocking)
void handleGPSStream() {
    while (GPSSerial.available()) {
        char c = GPSSerial.read();
        gps.encode(c);
        if (gps.location.isUpdated()) {
            updateLiveFix();
        }
    }
}

String formatLocation(double lat, double lng) {
    if (isnan(lat) || isnan(lng)) {
        return String("N/A");
    }
    String s;
    s += String(lat, 6);
    s += ",";
    s += String(lng, 6);
    return s;
}

String buildMessage(uint8_t which) {
    double lat = NAN, lng = NAN;
    if (haveLiveFix()) {
        lat = curLat;
        lng = curLng;
    } else if (!isnan(nvsLat) && !isnan(nvsLng)) {
        // If no live fix, fall back to last saved location
        lat = nvsLat;
        lng = nvsLng;
    }
    String loc = formatLocation(lat, lng);
    if (which == 1) {
        return String("HELP @ ") + loc;
    }
    if (which == 2) {
        return String("ENEMY SIGHTED @ ") + loc;
    }
    // Default for option 3 (current location)
    return String("CURRENT LOCATION @ ") + loc;
}
