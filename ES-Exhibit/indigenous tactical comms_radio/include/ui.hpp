#pragma once
#include <Arduino.h>
#include "display.hpp"

// Screen state enumeration
enum ScreenState {
    SCREEN_HOME = 0,
    SCREEN_RX,
    SCREEN_LOG_LIST,
    SCREEN_LOG_VIEW,
    SCREEN_COMPOSE
};

// Global UI state variables
extern ScreenState screen;
extern uint8_t logListPage;
extern String lastRx;
extern String rxFull;

// UI screen rendering functions
void drawHome();
void drawRxScreen();
void drawLogListScreen();
void drawLogViewScreen(int selectedSlot);

// Compose mode (multi-tap text input) functions
namespace Compose {
    void enter();
    void handleKey(char k);
    void autoCommitIfTimeout();
    void render();
}

// Quick message construction (with location)
String buildMessage(uint8_t which);
