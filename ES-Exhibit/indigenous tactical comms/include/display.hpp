#pragma once
#include "app_config.h"
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED display object (I2C)
extern Adafruit_SSD1306 display;

// Initialize the display (returns void; continues even if OLED not found)
void initDisplay();
