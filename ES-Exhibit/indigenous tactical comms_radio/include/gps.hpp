#pragma once
#include <Arduino.h>
#include <TinyGPSPlus.h>

// Global GPS objects
extern TinyGPSPlus gps;
extern HardwareSerial GPSSerial;

// Live GPS fix data (last known values)
extern double curLat;
extern double curLng;
extern uint32_t curSat;
extern double curHdop;
extern uint32_t lastFixMillis;

// Stored GPS fix from NVS (if live GPS is missing)
extern double nvsLat;
extern double nvsLng;

// Interval and timestamp for saving GPS fix to NVS
extern const uint32_t SAVE_INTERVAL;
extern uint32_t lastSave;

// GPS related functions
bool haveLiveFix();
void updateLiveFix();
void handleGPSStream();
void maybePersistFix();

String formatLocation(double lat, double lng);
