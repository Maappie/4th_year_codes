#include "ui.hpp"
#include "gps.hpp"       // for location data
#include "storage.hpp"   // for log data
#include "lora_radio.hpp"// for enqueueTx
#include <cstring>       // for strlen
#include "crypto.hpp"    // for TX_SENDER_TAG

// Global UI state variables
ScreenState screen = SCREEN_HOME;
uint8_t logListPage = 0;
String lastRx = "";
String rxFull = "";

// Truncate a string to at most 21 characters, add "…" if truncated
static String truncate21(const String& s) {
    if (s.length() <= 21) return s;
    return s.substring(0, 20) + "…";
}

// Draw text with simple wrapping (fixed width)
static void drawWrappedText(const String& s, int x, int y, uint8_t charsPerLine, uint8_t maxLines) {
    uint16_t start = 0;
    uint8_t line = 0;
    while (start < s.length() && line < maxLines) {
        uint16_t len = (charsPerLine < (s.length() - start) ? charsPerLine : (s.length() - start));
        display.setCursor(x, y + line * 8);
        display.print(s.substring(start, start + len));
        start += len;
        line++;
    }
}

// Home screen
void drawHome() {
    screen = SCREEN_HOME;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println(TX_SENDER_TAG);

    display.setCursor(0, 12);
    bool haveStored = (!isnan(nvsLat) && !isnan(nvsLng));
    bool live = gps.location.isValid();
    bool fresh = live && (millis() - lastFixMillis < SAVE_INTERVAL);
    if (fresh) {
        display.println("GPS: updated");
    } else if (haveStored) {
        display.println("GPS: not updated");
    } else {
        display.println("GPS: not fix");
    }

    display.setCursor(0, 24);
    display.println("1: Help");
    display.println("2: Enemy sighted");
    display.println("3: Current location");
    display.println("4: Custom message");
    display.println("C: View saved msgs");

    display.display();
}

// Received-message screen
void drawRxScreen() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("RECEIVED MESSAGE");
    drawWrappedText(rxFull, 0, 12, 21, 5);
    display.setCursor(0, 56);
    display.print("Press D to exit");
    display.display();
}

// Message log list screen
void drawLogListScreen() {
    screen = SCREEN_LOG_LIST;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.println("SELECT MSG 0-9");

    int startIdx = (logListPage == 0) ? 0 : 5;
    int endIdx   = startIdx + 5;
    for (int i = startIdx; i < endIdx; ++i) {
        int slot = userIndexToSlot(i);
        String label = String(i) + ". ";
        if (slot >= 0) {
            label += truncate21(logBuf[slot].sender);
        } else {
            label += "(empty)";
        }
        int line = i - startIdx;
        display.setCursor(0, 12 + line * 10);
        display.println(label);
    }

    display.setCursor(0, 56);
    display.print("#: Next  D: Back");
    display.display();
}

// Log view screen (view a selected message)
void drawLogViewScreen(int selectedSlot) {
    screen = SCREEN_LOG_VIEW;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.setCursor(0, 0);
    display.print("From: ");
    display.println(truncate21(logBuf[selectedSlot].sender));

    display.setCursor(0, 12);
    drawWrappedText(logBuf[selectedSlot].msg, 0, 12, 21, 5);

    display.setCursor(0, 56);
    display.print("D: Back");
    display.display();
}

// Compose mode (custom message input via multi-tap)
namespace Compose {
    static const unsigned long TAP_TIMEOUT = 700;
    static const size_t MAX_MSG_LEN = 512;
    static const char* MT_MAP[10] = {
        " ", ".,?!1", "ABC2", "DEF3", "GHI4",
        "JKL5", "MNO6", "PQRS7", "TUV8", "WXYZ9"
    };

    static String message = "";
    static char activeKey = 0;
    static uint8_t activeIdx = 0;
    static unsigned long lastTap = 0;
    static bool dirty = true;

    inline bool isDigitKey(char k) {
        return (k >= '0' && k <= '9');
    }

    static void commitActiveIfAny() {
        if (activeKey == 0) return;
        uint8_t group = activeKey - '0';
        char ch = MT_MAP[group][activeIdx];
        if (message.length() < MAX_MSG_LEN) {
            message += ch;
        }
        activeKey = 0;
        activeIdx = 0;
        dirty = true;
    }

    static void backspace() {
        if (activeKey != 0) {
            activeKey = 0;
            activeIdx = 0;
            dirty = true;
            return;
        }
        if (message.length() > 0) {
            message.remove(message.length() - 1);
            dirty = true;
        }
    }

    static void newline() {
        commitActiveIfAny();
        if (message.length() < MAX_MSG_LEN) {
            message += '\n';
            dirty = true;
        }
    }

    static void drawWrapped(const String& s, int x, int y, uint8_t charsPerLine, uint8_t maxLines) {
        uint16_t start = 0;
        uint8_t line = 0;
        while (start < s.length() && line < maxLines) {
            int nl = s.indexOf('\n', start);
            uint16_t len = (charsPerLine < (s.length() - start) ? charsPerLine : (s.length() - start));
            if (nl >= 0 && (uint16_t)(nl - start) < len) {
                len = nl - start;
            }
            display.setCursor(x, y + line * 8);
            display.print(s.substring(start, start + len));
            if (nl >= 0 && (uint16_t)(nl - (start + len)) == 0) {
                start = start + len + 1;
            } else {
                start += len;
            }
            line++;
        }
    }

    static String buildPreview() {
        if (activeKey == 0) return message;
        uint8_t group = activeKey - '0';
        char ch = MT_MAP[group][activeIdx];
        String s = message;
        if (s.length() < MAX_MSG_LEN) {
            s += ch;
        }
        return s;
    }

    void render() {
        if (!dirty) return;
        dirty = false;
        display.clearDisplay();
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);

        display.setCursor(0, 0);
        display.println("Custom Msg:");

        String preview = buildPreview();
        if (activeKey != 0 && preview.length() < MAX_MSG_LEN) {
            preview += '_';
        }
        drawWrapped(preview, 0, 10, 21, 6);
        display.setCursor(0, 56);
        display.print("*=Bksp  #=Newline  A=Send  D=Back");
        display.display();
    }

    void handleMultiTap(char k) {
        if (k == '0') {
            commitActiveIfAny();
            if (message.length() < MAX_MSG_LEN) {
                message += ' ';
                dirty = true;
            }
            return;
        }
        uint8_t group = k - '0';
        const char* seq = MT_MAP[group];
        uint8_t seqLen = strlen(seq);
        unsigned long now = millis();
        if (activeKey == k && (now - lastTap) <= TAP_TIMEOUT) {
            activeIdx = (activeIdx + 1) % seqLen;
            dirty = true;
        } else {
            commitActiveIfAny();
            activeKey = k;
            activeIdx = 0;
            dirty = true;
        }
        lastTap = now;
    }

    void handleKey(char k) {
        if (!k) return;
        if (!isDigitKey(k) && k != '*' && k != '#' && k != 'A' && k != 'D') {
            commitActiveIfAny();
        }
        switch (k) {
            case '*':
                backspace();
                break;
            case '#':
                newline();
                break;
            case 'A': {
                commitActiveIfAny();
                if (message.length() > 0) {
                    if (!enqueueTx(message)) {
                        Serial.println("[TXQ] Send refused: queue full");
                    }
                }
                message = "";
                activeKey = 0;
                activeIdx = 0;
                dirty = true;
                screen = SCREEN_HOME;
                drawHome();
                break;
            }
            case 'D':
                activeKey = 0;
                activeIdx = 0;
                screen = SCREEN_HOME;
                drawHome();
                break;
            default:
                if (isDigitKey(k)) {
                    handleMultiTap(k);
                }
                break;
        }
    }

    void autoCommitIfTimeout() {
        if (activeKey == 0) return;
        if (millis() - lastTap > TAP_TIMEOUT) {
            commitActiveIfAny();
        }
    }

    void enter() {
        screen = SCREEN_COMPOSE;
        dirty = true;
        render();
    }
}

String buildMessage(uint8_t which) {
    double lat = NAN, lng = NAN;
    if (haveLiveFix()) {
        lat = curLat;
        lng = curLng;
    } else if (!isnan(nvsLat) && !isnan(nvsLng)) {
        lat = nvsLat;
        lng = nvsLng;
    }
    String loc = formatLocation(lat, lng);
    if (which == 1) return String("HELP @ ") + loc;
    if (which == 2) return String("ENEMY SIGHTED @ ") + loc;
    return String("CURRENT LOCATION @ ") + loc;
}
