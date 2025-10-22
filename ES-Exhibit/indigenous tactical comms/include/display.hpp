#pragma once
#include <Adafruit_SSD1306.h>
#include <Arduino.h>

// Global display
extern Adafruit_SSD1306 display;

// UI helpers (same names)
String truncate21(const String& s);
void drawWrappedText(const String& s, int x, int y, uint8_t charsPerLine, uint8_t maxLines);

// Screens (same names)
void drawRxScreen();
void drawHome();
void drawLogListScreen();
void drawLogViewScreen(int selectedSlot);
