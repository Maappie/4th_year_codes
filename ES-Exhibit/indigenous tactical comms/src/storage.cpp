#include "storage.hpp"
#include <Preferences.h>

// NVS preferences (shared for GPS and log)
Preferences prefs;

// In-RAM shadow of NVS log
static uint8_t logCount = 0;  // number of valid entries (0..LOG_CAP)
static uint8_t logHead  = 0;  // index of next write position (circular buffer)

MsgEntry logBuf[LOG_CAP];

// NVS helpers for the message log
void loadMsgLogFromNVS() {
    if (!prefs.begin("msglog", true)) return;
    logCount = prefs.getUChar("cnt", 0);
    logHead  = prefs.getUChar("head", 0);
    if (logCount > LOG_CAP) logCount = LOG_CAP;
    if (logHead >= LOG_CAP) logHead = 0;
    for (int i = 0; i < LOG_CAP; ++i) {
        String sKey = String("s") + i;
        String mKey = String("m") + i;
        String nKey = String("n") + i;
        logBuf[i].sender   = prefs.getString(sKey.c_str(), "");
        logBuf[i].msg      = prefs.getString(mKey.c_str(), "");
        logBuf[i].nonceHex = prefs.getString(nKey.c_str(), "");
    }
    prefs.end();
}

static void saveMsgLogSlotToNVS(int slot) {
    if (!prefs.begin("msglog", false)) return;
    String sKey = String("s") + slot;
    String mKey = String("m") + slot;
    String nKey = String("n") + slot;
    prefs.putString(sKey.c_str(), logBuf[slot].sender);
    prefs.putString(mKey.c_str(), logBuf[slot].msg);
    prefs.putString(nKey.c_str(), logBuf[slot].nonceHex);
    prefs.end();
}

static void saveMsgLogMetaToNVS() {
    if (!prefs.begin("msglog", false)) return;
    prefs.putUChar("cnt", logCount);
    prefs.putUChar("head", logHead);
    prefs.end();
}

// Add new entry (authenticated RX). Newest is index 0 for user view.
void addLogEntryPersistent(const String& sender, const String& message, const String& nonceHex) {
    logBuf[logHead].sender   = sender;
    logBuf[logHead].msg      = message;
    logBuf[logHead].nonceHex = nonceHex;
    saveMsgLogSlotToNVS(logHead);
    logHead = (logHead + 1) % LOG_CAP;
    if (logCount < LOG_CAP) logCount++;
    saveMsgLogMetaToNVS();
}

// Map a user-facing index (0=newest .. 9=oldest) to the circular slot.
// Returns -1 if that index is out of range for current logCount.
int userIndexToSlot(int userIdx) {
    if (userIdx < 0 || userIdx >= logCount) return -1;
    int newestSlot = (int)logHead - 1;
    if (newestSlot < 0) newestSlot += LOG_CAP;
    int slot = newestSlot - userIdx;
    while (slot < 0) slot += LOG_CAP;
    return slot % LOG_CAP;
}
