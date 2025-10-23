#include "keypad.hpp"
#include "ui.hpp"         // for UI state and Compose functions
#include "lora_radio.hpp" // for enqueueTx and log data
#include "gps.hpp"        // for buildMessage dependencies
#include "storage.hpp"    // for logBuf and userIndexToSlot

// Keypad layout setup (4x4 matrix)
static byte rowPins[4] = {32, 33, 25, 4};    // Row pins (R1..R4)
static byte colPins[4] = {27, 12, 13, 15};   // Column pins (C1..C4)
static char keys[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};
static Keypad keypad = Keypad(makeKeymap(keys), rowPins, colPins, 4, 4);

// Poll the keypad and handle keypress events
void handleKeypad() {
    char k = keypad.getKey();
    if (!k) return;

    // If in compose mode, delegate to Compose handler
    if (screen == SCREEN_COMPOSE) {
        Compose::handleKey(k);
        return;
    }
    // Handle key based on current screen state
    switch (screen) {
        case SCREEN_RX:
            if (k == 'D') {
                drawHome();
            }
            return;
        case SCREEN_LOG_LIST:
            if (k == 'D') {
                drawHome();
                return;
            }
            if (k == '#') {
                // Toggle between first 5 and next 5 log entries
                logListPage ^= 1;
                drawLogListScreen();
                return;
            }
            if (k >= '0' && k <= '9') {
                int keyNum = k - '0';
                int minIdx = (logListPage == 0) ? 0 : 5;
                int maxIdx = (logListPage == 0) ? 4 : 9;
                if (keyNum >= minIdx && keyNum <= maxIdx) {
                    int slot = userIndexToSlot(keyNum);
                    if (slot >= 0) {
                        Serial.print("[OPEN SLOT ");
                        Serial.print(keyNum);
                        Serial.print("] sender=");
                        Serial.print(logBuf[slot].sender);
                        Serial.print(" nonce=");
                        Serial.println(logBuf[slot].nonceHex);
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
    // Handle keys on the home screen for quick actions
    switch (k) {
        case '1':
        case '2':
        case '3': {
            uint8_t which = static_cast<uint8_t>(k - '0');
            String msg = buildMessage(which);
            if (!enqueueTx(msg)) {
                Serial.println("[TXQ] Send refused: queue full");
            } else {
                lastRx = "TX: " + msg;
            }
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
            // Ignore other keys
            break;
    }
}
