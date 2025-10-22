#include "ui.hpp"
#include "display.hpp"
#include "gps.hpp"
#include "storage.hpp"
#include "lora_radio.hpp"
#include "app_config.h"

#include <Arduino.h>

// Shared cadence from original
uint32_t lastSave = 0;

// Screen state
ScreenState screen = SCREEN_HOME;

// Build message (same logic)
String formatLocation(double lat, double lng); // from gps.cpp
String buildMessage(uint8_t which) {
  double lat = NAN, lng = NAN;
  if (haveLiveFix()) { lat = curLat; lng = curLng; }
  else if (!isnan(nvsLat) && !isnan(nvsLng)) { lat = nvsLat; lng = nvsLng; }

  String loc = formatLocation(lat, lng);
  if (which == 1) return "HELP @ " + loc;
  if (which == 2) return "ENEMY SIGHTED @ " + loc;
  return "CURRENT LOCATION @ " + loc;
}

// === Compose namespace (exact same behavior) ===
namespace Compose {

  static const unsigned long TAP_TIMEOUT = 700;
  static const size_t MAX_MSG_LEN = 512;

  static const char* MT_MAP[10] = {
    " ", ".,?!1", "ABC2", "DEF3", "GHI4",
    "JKL5", "MNO6", "PQRS7", "TUV8", "WXYZ9"
  };

  static String message = "";
  static char   activeKey = 0;
  static uint8_t activeIdx = 0;
  static unsigned long lastTap = 0;
  static bool dirty = true;

  inline bool isDigitKey(char k){ return (k >= '0' && k <= '9'); }

  void commitActiveIfAny() {
    if (activeKey == 0) return;
    uint8_t group = activeKey - '0';
    char ch = MT_MAP[group][activeIdx];
    if (message.length() < MAX_MSG_LEN) message += ch;
    activeKey = 0;
    activeIdx = 0;
    dirty = true;
  }

  void backspace() {
    if (activeKey != 0) { activeKey = 0; activeIdx = 0; dirty = true; return; }
    if (message.length() > 0) { message.remove(message.length() - 1); dirty = true; }
  }

  void newline() {
    commitActiveIfAny();
    if (message.length() < MAX_MSG_LEN) { message += '\n'; dirty = true; }
  }

  // local drawWrapped with newline support exactly like before
  static void drawWrapped(const String& s, int x, int y, uint8_t charsPerLine, uint8_t maxLines) {
    uint16_t start = 0;
    uint8_t line = 0;
    while (start < s.length() && line < maxLines) {
      int nl = s.indexOf('\n', start);
      uint16_t len = min<uint16_t>(charsPerLine, s.length() - start);
      if (nl >= 0 && (uint16_t)(nl - start) < len) len = nl - start;

      display.setCursor(x, y + line * 8);
      display.print(s.substring(start, start + len));
      start += len;

      if (nl >= 0 && (uint16_t)(nl - (start - len)) == 0) start++;
      line++;
    }
  }

  String buildPreview() {
    if (activeKey == 0) return message;
    uint8_t group = activeKey - '0';
    char ch = MT_MAP[group][activeIdx];
    String s = message;
    if (s.length() < MAX_MSG_LEN) s += ch;
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
    if (activeKey != 0 && preview.length() < MAX_MSG_LEN) preview += '_';

    drawWrapped(preview, 0, 10, 21, 6);

    display.setCursor(0, 56);
    display.print("*=Bksp  #=Newline  A=Send  D=Back");
    display.display();
  }

  void handleMultiTap(char k) {
    if (k == '0') {
      commitActiveIfAny();
      if (message.length() < MAX_MSG_LEN) message += ' ';
      dirty = true;
      return;
    }

    uint8_t group = k - '0';
    const char* seq = MT_MAP[group];
    uint8_t seqLen = strlen(seq);
    unsigned long now = millis();

    if (activeKey == k && (now - lastTap) <= TAP_TIMEOUT) {
      activeIdx = (activeIdx + 1) % seqLen;  // cycle
      dirty = true;
    } else {
      commitActiveIfAny();
      activeKey = k; activeIdx = 0;
      dirty = true;
    }
    lastTap = now;
  }

  void handleKey(char k) {
    if (!k) return;

    if (!isDigitKey(k) && k != '*' && k != '#'
        && k != 'A' && k != 'D') {
      commitActiveIfAny();
    }

    switch (k) {
      case '*': backspace(); break;
      case '#': newline();   break;
      case 'A': {
        commitActiveIfAny();
        if (message.length() > 0) {
          loraSend(message);
        }
        message = "";
        activeKey = 0; activeIdx = 0;
        dirty = true;
        screen = SCREEN_HOME;
        drawHome();
        break;
      }
      case 'D':
        activeKey = 0; activeIdx = 0;
        screen = SCREEN_HOME;
        drawHome();
        break;
      default:
        if (isDigitKey(k)) handleMultiTap(k);
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
} // namespace Compose

// Keypad routing (same behavior)
#include "keypad.hpp"

void handleKeypad() {
  char k = keypad.getKey();
  if (!k) return;

  if (screen == SCREEN_COMPOSE) { Compose::handleKey(k); return; }

  switch (screen) {
    case SCREEN_RX:
      if (k == 'D') drawHome();
      return;

    case SCREEN_LOG_LIST:
      if (k == 'D') { drawHome(); return; }
      if (k == '#') { logListPage ^= 1; drawLogListScreen(); return; }
      if ((k >= '0') && (k <= '9')) {
        int keyNum = k - '0';
        int minIdx = (logListPage == 0) ? 0 : 5;
        int maxIdx = (logListPage == 0) ? 4 : 9;
        if (keyNum >= minIdx && keyNum <= maxIdx) {
          int slot = userIndexToSlot(keyNum);
          if (slot >= 0) {
            Serial.print("[OPEN SLOT "); Serial.print(keyNum);
            Serial.print("] sender="); Serial.print(logBuf[slot].sender);
            Serial.print(" nonce=");  Serial.println(logBuf[slot].nonceHex);
            drawLogViewScreen(slot);
          }
        }
      }
      return;

    case SCREEN_LOG_VIEW:
      if (k == 'D') { drawLogListScreen(); }
      return;

    case SCREEN_HOME:
    default:
      break;
  }

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
