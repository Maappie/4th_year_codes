#include "keypad.hpp"
#include <Arduino.h>

byte rowPins[4] = {32, 33, 25, 4};
byte colPins[4] = {27, 12, 13, 15};

char keys[4][4] = {
  {'1','2','3','A'},
  {'4','5','6','B'},
  {'7','8','9','C'},
  {'*','0','#','D'}
};

Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);
