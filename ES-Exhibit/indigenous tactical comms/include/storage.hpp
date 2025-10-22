#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Shared Preferences instance (defined in storage.cpp)
extern Preferences prefs;

// Message log
struct MsgEntry {
  String sender;
  String msg;
  String nonceHex;
};

extern const int LOG_CAP;
extern MsgEntry logBuf[];
extern uint8_t logCount;
extern uint8_t logHead;
extern uint8_t logListPage;

// NVS helpers
void loadMsgLogFromNVS();
void saveMsgLogSlotToNVS(int slot);
void saveMsgLogMetaToNVS();

// API
void addLogEntryPersistent(const String& sender, const String& message, const String& nonceHex);
int userIndexToSlot(int userIdx);
