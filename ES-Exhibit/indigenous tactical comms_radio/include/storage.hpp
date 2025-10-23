#pragma once
#include <Arduino.h>
#include <Preferences.h>

// Persistent message log entry structure
struct MsgEntry {
    String sender;
    String msg;
    String nonceHex;
};

// Log buffer and metadata
extern const uint8_t LOG_CAP;
extern MsgEntry logBuf[];
extern uint8_t logCount;
extern uint8_t logHead;

// Preferences handle for NVS storage
extern Preferences prefs;

// Storage (NVS) functions for GPS and message log persistence
void loadNVS();
void saveNVS(double lat, double lng);
void loadMsgLogFromNVS();
void saveMsgLogSlotToNVS(int slot);
void saveMsgLogMetaToNVS();
void addLogEntryPersistent(const String& sender, const String& message, const String& nonceHex);
int  userIndexToSlot(int userIdx);
