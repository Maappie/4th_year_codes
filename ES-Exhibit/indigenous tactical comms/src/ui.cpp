// src/ui.cpp
#include "ui.hpp"
#include "storage.hpp"
#include "gps.hpp"
#include <algorithm>
#include "crypto.hpp"   

ScreenState screen = SCREEN_HOME;
uint8_t logListPage = 0;
String lastRx = "";
String rxFull = "";

// Truncate string to 21 characters (for display line width)
static String truncate21(const String& s) {
    if (s.length() <= 21) {
        return s;
    }
    return s.substring(0, 20) + "…";
}

// Draw wrapped text on display (given width and max lines)
static void drawWrappedText(const String& s, int x, int y, uint8_t charsPerLine, uint8_t maxLines) {
    uint16_t start = 0;
    uint8_t line = 0;
    while (start < s.length() && line < maxLines) {
        uint16_t len = (uint16_t)std::min<uint16_t>(charsPerLine, s.length() - start);
        display.setCursor(x, y + line * 8);
        display.print(s.substring(start, start + len));
        start += len;
        line++;
    }
}

// Display the RX (received message) screen
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

// Display the home screen with menu and GPS status
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
    } else if (live || haveStored) {
        display.println("GPS: not updated");
    } else {
        display.println("GPS: no fix");
    }
    display.setCursor(0, 24);
    display.println("1: Help");
    display.println("2: Enemy sighted");
    display.println("3: Current location");
    display.println("4: Custom message");
    display.println("C: View saved msgs");
    display.display();
}

// Display the message log list (pagination)
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
        String label = String(i) + String(". ");
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

// Display a single log entry view
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
