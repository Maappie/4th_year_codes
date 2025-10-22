#pragma once
#include <Arduino.h>

// Screen state (same names/values)
enum ScreenState { SCREEN_HOME = 0, SCREEN_RX = 1, SCREEN_LOG_LIST = 2, SCREEN_LOG_VIEW = 3, SCREEN_COMPOSE = 4 };
extern ScreenState screen;

// Shared cadence timer from original sketch
extern uint32_t lastSave;

// Compose namespace API (same names)
namespace Compose {
  void enter();
  void render();
  void handleKey(char k);
  void autoCommitIfTimeout();
}

// Home helpers used by keypad
String buildMessage(uint8_t which);

// Keypad route
void handleKeypad();
