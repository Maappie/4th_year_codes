#ifndef KEYPAD_MODULE_H
#define KEYPAD_MODULE_H

#include <Arduino.h>

// Keypad handling and message composition
void handleKeypad();

namespace Compose {
    void autoCommitIfTimeout();
    void render();
}

#endif // KEYPAD_MODULE_H
