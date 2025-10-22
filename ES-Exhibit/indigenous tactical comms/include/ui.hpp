#pragma once
#include "app_config.h"
#include <Arduino.h>

// UI state screens
enum ScreenState { SCREEN_HOME = 0, SCREEN_RX = 1, SCREEN_LOG_LIST = 2, SCREEN_LOG_VIEW = 3, SCREEN_COMPOSE = 4 };
extern ScreenState screen;
extern uint8_t logListPage;
extern String lastRx;
extern String rxFull;

// UI screens and interface
void drawHome();
void drawRxScreen();
void drawLogListScreen();
void drawLogViewScreen(int selectedSlot);
String buildMessage(uint8_t which);

// Compose (custom message composer) namespace interface
namespace Compose {
    void handleKey(char k);
    void autoCommitIfTimeout();
    void render();
    void enter();
}
