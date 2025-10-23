#pragma once
#include <Arduino.h>

// LoRa radio pins and frequency
static const int PIN_LORA_SS   = 5;
static const int PIN_LORA_RST  = 14;
static const int PIN_LORA_DIO0 = 26;

#ifndef LORA_BAND
  #define LORA_BAND 433E6  // default 433 MHz band if not defined
#endif
static const long LORA_FREQ = LORA_BAND;

// GPS pins
static const int GPS_RX = 16; // ESP32 RX2 (connect to GPS TX)
static const int GPS_TX = 17; // ESP32 TX2 (optional)

// OLED I2C params (prefer build_flags; only define if missing)
#ifndef SCREEN_WIDTH
  #define SCREEN_WIDTH 128
#endif
#ifndef SCREEN_HEIGHT
  #define SCREEN_HEIGHT 64
#endif
#ifndef OLED_ADDR
  #define OLED_ADDR 0x3C
#endif
#ifndef OLED_RESET
  #define OLED_RESET -1
#endif
