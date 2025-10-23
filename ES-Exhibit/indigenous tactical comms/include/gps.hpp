#ifndef GPS_MODULE_H
#define GPS_MODULE_H

#include <Arduino.h>
#include <TinyGPSPlus.h>

// GPS interface and data
extern TinyGPSPlus gps;
extern HardwareSerial GPSSerial;
extern double curLat;
extern double curLng;
extern uint32_t curSat;
extern double curHdop;
extern uint32_t lastFixMillis;

// GPS processing functions
bool haveLiveFix();
void updateLiveFix();
void handleGPSStream();
String formatLocation(double lat, double lng);
String buildMessage(uint8_t which);

#endif // GPS_MODULE_H
