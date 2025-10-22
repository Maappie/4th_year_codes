#pragma once
#include <Arduino.h>
#include <TinyGPSPlus.h>

// Exposed globals/state
extern TinyGPSPlus gps;
extern HardwareSerial GPSSerial;

extern double curLat, curLng;
extern uint32_t curSat;
extern double curHdop;
extern uint32_t lastFixMillis;

extern double nvsLat, nvsLng;

// NMEA batching (same behavior)
void printNMEADump1Hz();

// API
void loadNVS();
void saveNVS(double lat, double lng);
bool haveLiveFix();
void updateLiveFix();
void handleGPSStream();
String formatLocation(double lat, double lng);
void maybePersistFix();
