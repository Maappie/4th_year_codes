#pragma once
#include <Arduino.h>
#include <cmath>  // for NAN

// --- PIN MAP (edit to match your wiring) ---
static const int PIN_LORA_SS   = 5;
static const int PIN_LORA_RST  = 14;
static const int PIN_LORA_DIO0 = 26;
#ifndef LORA_BAND
#define LORA_BAND 433E6         // 433E6 from build_flags
#endif
static const long LORA_FREQ    = LORA_BAND;
// SPI uses SCK=18, MISO=19, MOSI=23 by default on ESP32

// GPS on Serial2
static const int GPS_RX = 16;   // ESP32 RX2 (connect to GPS TX)
static const int GPS_TX = 17;   // ESP32 TX2 (optional, to GPS RX)

// OLED I2C configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_ADDR 0x3C
#define OLED_RESET -1

// Keypad 4x4 mapping
static const byte ROWS = 4;
static const byte COLS = 4;
static const byte R1_PIN = 32;
static const byte R2_PIN = 33;
static const byte R3_PIN = 25;
static const byte R4_PIN = 4;
static const byte C1_PIN = 27;
static const byte C2_PIN = 12;
static const byte C3_PIN = 13;
static const byte C4_PIN = 15;

// Unit identity and crypto labels
static const char* TX_SENDER_TAG = "Radio 1";
static const char* TX_KEY_LABEL  = "unit-001";
static const char* RX_KEYS_LABELS[] = {"HQ-PH", "unit-002"};
static const size_t RX_KEYS_COUNT = sizeof(RX_KEYS_LABELS) / sizeof(RX_KEYS_LABELS[0]);

// Timing constants
static const uint32_t SAVE_INTERVAL = 5UL * 60UL * 1000UL;  // 5 minutes
