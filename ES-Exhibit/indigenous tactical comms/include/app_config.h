// app_config.h
#ifndef APP_CONFIG_H
#define APP_CONFIG_H
#include <Arduino.h>

// ---------- PIN MAP ----------
static const int PIN_LORA_SS   = 5;
static const int PIN_LORA_RST  = 14;
static const int PIN_LORA_DIO0 = 26;

// SPI default pins: SCK=18, MISO=19, MOSI=23

// GPS on Serial2
static const int GPS_RX = 16;
static const int GPS_TX = 17;

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1

// LoRa frequency from build flag (e.g., -D LORA_BAND=433E6)
#ifndef LORA_BAND
#error "LORA_BAND must be defined in build_flags (e.g., 433E6)"
#endif
static const long LORA_FREQ = LORA_BAND;

static const uint32_t SAVE_INTERVAL = 5UL * 60UL * 1000UL;

#endif
