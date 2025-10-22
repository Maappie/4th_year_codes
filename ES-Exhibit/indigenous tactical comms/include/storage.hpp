#pragma once
#include "app_config.h"
#include <Arduino.h>
#include <Preferences.h>

// ---------- PERSISTENT LAST-10 MESSAGE LOG (NVS) ----------
static const int LOG_CAP = 10;
struct MsgEntry {
    String sender;
    String msg;
    String nonceHex;
};

extern MsgEntry logBuf[LOG_CAP];
extern Preferences prefs;

// Message log storage functions
void loadMsgLogFromNVS();
void addLogEntryPersistent(const String& sender, const String& message, const String& nonceHex);
int userIndexToSlot(int userIdx);
