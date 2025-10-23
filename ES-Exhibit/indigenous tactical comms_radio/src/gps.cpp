#include "gps.hpp"
#include "storage.hpp"

// Initialize global GPS-related objects and variables
TinyGPSPlus gps;
HardwareSerial GPSSerial(2);  // Serial2 for GPS

double curLat = NAN;
double curLng = NAN;
uint32_t curSat = 0;
double curHdop = NAN;
uint32_t lastFixMillis = 0;

double nvsLat = NAN;
double nvsLng = NAN;

const uint32_t SAVE_INTERVAL = 5UL * 60UL * 1000UL; // 5 minutes in milliseconds
uint32_t lastSave = 0;

bool haveLiveFix() {
    // Check if GPS location is valid and recent (<5 seconds old)
    return gps.location.isValid() && gps.location.age() < 5000;
}

void updateLiveFix() {
    // Update current live fix data if GPS has a valid location
    if (gps.location.isValid()) {
        curLat = gps.location.lat();
        curLng = gps.location.lng();
        curSat = gps.satellites.value();
        curHdop = gps.hdop.hdop();
        lastFixMillis = millis();
    }
}

void handleGPSStream() {
    // Read from GPS serial and feed into TinyGPSPlus parser
    while (GPSSerial.available()) {
        char c = GPSSerial.read();
        gps.encode(c);
        if (gps.location.isUpdated()) {
            updateLiveFix();
        }
    }
}

void maybePersistFix() {
  if (!haveLiveFix()) return;
  if (lastSave == 0 || (millis() - lastSave) >= SAVE_INTERVAL) {
    bool bootSave = (lastSave == 0);
    (void)bootSave; // keep for clarity
    saveNVS(curLat, curLng);
    nvsLat = curLat;
    nvsLng = curLng;
    lastSave = millis();
  }
}


String formatLocation(double lat, double lng) {
    if (isnan(lat) || isnan(lng)) {
        return String("N/A");
    }
    String s = String(lat, 6);
    s += ",";
    s += String(lng, 6);
    return s;
}
