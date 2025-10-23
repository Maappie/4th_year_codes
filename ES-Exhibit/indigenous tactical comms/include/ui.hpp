#ifndef UI_MODULE_H
#define UI_MODULE_H

#include <Arduino.h>
#include "display.hpp"

// Screen states for UI
enum ScreenState { SCREEN_HOME = 0, SCREEN_RX, SCREEN_LOG_LIST, SCREEN_LOG_VIEW, SCREEN_COMPOSE };
extern ScreenState screen;
extern uint8_t logListPage;
extern String lastRx;
extern String rxFull;

// UI rendering functions
void drawHome();
void drawLogListScreen();
void drawLogViewScreen(int selectedSlot);
void drawRxScreen();

#endif // UI_MODULE_H
