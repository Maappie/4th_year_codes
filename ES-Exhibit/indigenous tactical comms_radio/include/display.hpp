#pragma once
#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Global display object for the OLED
extern Adafruit_SSD1306 display;

// Display-related initialization function
bool initDisplay();
