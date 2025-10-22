#pragma once
#include "app_config.h"
#include <TinyGPSPlus.h>

// GPS and location management
extern TinyGPSPlus gps;
extern HardwareSerial GPSSerial;
extern double curLat;
extern double curLng;
extern uint32_t curSat;
extern double curHdop;
extern uint32_t lastFixMillis;
extern double nvsLat;
extern double nvsLng;

// GPS / NVS (GPS) function declarations
void loadNVS();             // Load last saved GPS coords from NVS
void saveNVS(double lat, double lng);
bool haveLiveFix();
void updateLiveFix();
void printNMEADump1Hz();
void handleGPSStream();
void maybePersistFix();
String formatLocation(double lat, double lng);
