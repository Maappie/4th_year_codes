#ifndef STORAGE_MODULE_H
#define STORAGE_MODULE_H

#include <Arduino.h>
#include <Preferences.h>

// Message log entry structure
struct MsgEntry {
    String sender;
    String msg;
    String nonceHex;
};

// Log capacity
constexpr size_t LOG_CAP = 10;

// Persistent storage (NVS) for message log
void loadMsgLogFromNVS();
void saveMsgLogSlotToNVS(int slot);
void saveMsgLogMetaToNVS();
void addLogEntryPersistent(const String& sender, const String& message, const String& nonceHex);

// Message log data (circular buffer)
extern MsgEntry logBuf[LOG_CAP];

// Log indexing helper
int userIndexToSlot(int userIdx);

// Preferences (NVS) global storage
extern Preferences prefs;

// GPS last location persistence
void loadNVS();
void saveNVS(double lat, double lng);

// Last known saved GPS coordinates
extern double nvsLat;
extern double nvsLng;

#endif // STORAGE_MODULE_H
