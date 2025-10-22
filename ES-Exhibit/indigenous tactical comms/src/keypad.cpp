#include "keypad.hpp"
#include "ui.hpp"
#include "lora_radio.hpp"
#include "storage.hpp"
#include <Keypad.h>

// Keypad hardware setup
static byte rowPins[ROWS] = {R1_PIN, R2_PIN, R3_PIN, R4_PIN};
static byte colPins[COLS] = {C1_PIN, C2_PIN, C3_PIN, C4_PIN};
static char keys[ROWS][COLS] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};
static Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, ROWS, COLS);

void handleKeypad() {
    char k = keypad.getKey();
    if (!k) return;
    if (screen == SCREEN_COMPOSE) {
        Compose::handleKey(k);
        return;
    }
    switch (screen) {
        case SCREEN_RX:
            if (k == 'D') drawHome();
            return;
        case SCREEN_LOG_LIST:
            if (k == 'D') { drawHome(); return; }
            if (k == '#') {
                logListPage ^= 1;
                drawLogListScreen();
                return;
            }
            if (k >= '0' && k <= '9') {
                int keyNum = k - '0';
                int startIdx = (logListPage == 0 ? 0 : 5);
                int endIdx   = (logListPage == 0 ? 4 : 9);
                if (keyNum >= startIdx && keyNum <= endIdx) {
                    int slot = userIndexToSlot(keyNum);
                    if (slot >= 0) {
                        Serial.print("[OPEN SLOT ");
                        Serial.print(keyNum);
                        Serial.print("] sender="); Serial.print(logBuf[slot].sender);
                        Serial.print(" nonce=");  Serial.println(logBuf[slot].nonceHex);
                        drawLogViewScreen(slot);
                    }
                }
            }
            return;
        case SCREEN_LOG_VIEW:
            if (k == 'D') {
                drawLogListScreen();
            }
            return;
        case SCREEN_HOME:
        default:
            break;
    }
    // Handle home screen key options
    switch (k) {
        case '1':
        case '2':
        case '3': {
            uint8_t which = static_cast<uint8_t>(k - '0');
            String msg = buildMessage(which);
            loraSend(msg);
            lastRx = "TX: " + msg;
            drawHome();
            break;
        }
        case '4':
            Compose::enter();
            break;
        case 'C':
            logListPage = 0;
            drawLogListScreen();
            break;
        default:
            break;
    }
}
