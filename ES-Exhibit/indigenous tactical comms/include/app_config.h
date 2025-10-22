#pragma once
#include <stddef.h>  
#include <stdint.h>  

// -------- Project-wide config & pin map --------

// LoRa pins & band (LORA_BAND comes from build_flags)
static const int PIN_LORA_SS   = 5;
static const int PIN_LORA_RST  = 14;
static const int PIN_LORA_DIO0 = 26;
static const long LORA_FREQ    = LORA_BAND;  // e.g., 433E6

// SPI default on ESP32: SCK=18, MISO=19, MOSI=23

// GPS on Serial2
static const int GPS_RX = 16; // ESP32 RX2  (connect to GPS TX)
static const int GPS_TX = 17; // ESP32 TX2  (optional)

// OLED I2C (default SDA=21, SCL=22)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1

// Tag shown on title and used as AAD for AES-GCM
static const char* TX_SENDER_TAG = "Command Center";

// Key labels
static const char* TX_KEY_LABEL  = "HQ-PH";
static const char* RX_KEYS_LABELS[] = {"unit-001", "unit-002"};
static const size_t RX_KEYS_COUNT = sizeof(RX_KEYS_LABELS)/sizeof(RX_KEYS_LABELS[0]);

// GPS save cadence
const uint32_t SAVE_INTERVAL = 5UL * 60UL * 1000UL;
