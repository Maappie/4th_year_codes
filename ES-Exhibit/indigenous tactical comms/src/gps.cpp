#include "gps.hpp"
#include "app_config.h"
#include "storage.hpp"
#include "ui.hpp"   

TinyGPSPlus gps;
HardwareSerial GPSSerial(2);

// live fix
double curLat = NAN, curLng = NAN;
uint32_t curSat = 0;
double curHdop = NAN;
uint32_t lastFixMillis = 0;

// persisted
double nvsLat = NAN, nvsLng = NAN;

// --- GPS raw NMEA capture (print 1 Hz) ---
const uint8_t NMEA_BATCH_CAP = 12;
static String nmeaBatch[NMEA_BATCH_CAP];
static uint8_t nmeaBatchCount = 0;
static String nmeaCurLine = "";
static uint32_t lastNmeaPrint = 0;

void loadNVS() {
  if (!prefs.begin("gps", true)) {
    Serial.println("Creating new NVS namespace...");
    prefs.begin("gps", false);
    prefs.putDouble("lat", NAN);
    prefs.putDouble("lng", NAN);
  } else {
    nvsLat = prefs.getDouble("lat", NAN);
    nvsLng = prefs.getDouble("lng", NAN);
  }
  prefs.end();
}

void saveNVS(double lat, double lng) {
  prefs.begin("gps", false);
  prefs.putDouble("lat", lat);
  prefs.putDouble("lng", lng);
  prefs.end();
}

bool haveLiveFix() {
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

void printNMEADump1Hz() {
  if (millis() - lastNmeaPrint >= 1000) {
    Serial.println("[GPS RAW @1s]");
    if (nmeaBatchCount == 0) {
      Serial.println("(no new NMEA)");
    } else {
      for (uint8_t i = 0; i < nmeaBatchCount; ++i) {
        Serial.println(nmeaBatch[i]);
      }
      nmeaBatchCount = 0;
    }
    lastNmeaPrint = millis();
  }
}

void handleGPSStream() {
  while (GPSSerial.available()) {
    char c = GPSSerial.read();

    gps.encode(c);
    if (gps.location.isUpdated()) updateLiveFix();

    if (c == '\r') {
      // ignore
    } else if (c == '\n') {
      if (nmeaCurLine.length() > 0) {
        if (nmeaBatchCount < NMEA_BATCH_CAP) {
          nmeaBatch[nmeaBatchCount++] = nmeaCurLine;
        }
        nmeaCurLine = "";
      }
    } else {
      if (nmeaCurLine.length() < 120) nmeaCurLine += c;
    }
  }
}

String formatLocation(double lat, double lng) {
  if (isnan(lat) || isnan(lng)) return String("N/A");
  String s = "";
  s += String(lat, 6); s += ","; s += String(lng, 6);
  return s;
}

void maybePersistFix() {
  static uint32_t lastNoFixLog = 0;

  if (!haveLiveFix()) {
    if (millis() - lastNoFixLog >= 1000) {
      Serial.println("no valid live fix");
      lastNoFixLog = millis();
    }
    return;
  }

  if (lastSave == 0 || (millis() - lastSave) >= SAVE_INTERVAL) {
    bool bootSave = (lastSave == 0);
    saveNVS(curLat, curLng);
    nvsLat = curLat; nvsLng = curLng;
    lastSave = millis();
    Serial.println(bootSave ? "[NVS] Saved (boot)" : "[NVS] Periodic GPS saved");
  }
}
