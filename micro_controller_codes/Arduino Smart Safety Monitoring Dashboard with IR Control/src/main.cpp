#include <IRremote.h>
#include <EEPROM.h>

const int flamePin  = 2;
const int gasPin    = A0;
const int motionPin = 6;
const int relayPin  = 4;
const int irPin     = 3;

const uint32_t CODE_ON  = 0xE9167B80;
const uint32_t CODE_OFF = 0xE8177B80;

unsigned long lastLogTime = 0;
bool prevHazard = false;

struct LogEntry {
  uint32_t ms;
  uint16_t gas;
  uint8_t  flags;
  uint8_t  code;
};

const int EE_ADDR_IDX   = 0;
const int EE_ADDR_COUNT = 2;
const int EE_ADDR_DATA  = 4;

const int ENTRY_SIZE = sizeof(LogEntry);
const int EE_BYTES   = 1024;
const int MAX_ENTRIES = (EE_BYTES - EE_ADDR_DATA) / ENTRY_SIZE;

uint16_t eeReadU16(int addr) {
  uint16_t v;
  EEPROM.get(addr, v);
  return v;
}
void eeWriteU16(int addr, uint16_t v) {
  EEPROM.put(addr, v);
}

void eepromInitIfEmpty() {
  uint16_t idx = eeReadU16(EE_ADDR_IDX);
  uint16_t cnt = eeReadU16(EE_ADDR_COUNT);
  if (idx >= MAX_ENTRIES || cnt > MAX_ENTRIES) {
    eeWriteU16(EE_ADDR_IDX, 0);
    eeWriteU16(EE_ADDR_COUNT, 0);
  }
}

void eepromLog(uint8_t eventCode, uint16_t gasVal, bool flame, bool motion, bool relay, bool hazard) {
  uint16_t idx = eeReadU16(EE_ADDR_IDX);
  uint16_t cnt = eeReadU16(EE_ADDR_COUNT);
  LogEntry e;
  e.ms    = millis();
  e.gas   = gasVal;
  uint8_t f = 0;
  if (flame)  f |= (1 << 0);
  if (motion) f |= (1 << 1);
  if (relay)  f |= (1 << 2);
  if (hazard) f |= (1 << 3);
  e.flags = f;
  e.code  = eventCode;
  int entryAddr = EE_ADDR_DATA + idx * ENTRY_SIZE;
  EEPROM.put(entryAddr, e);
  idx = (idx + 1) % MAX_ENTRIES;
  if (cnt < MAX_ENTRIES) cnt++;
  eeWriteU16(EE_ADDR_IDX, idx);
  eeWriteU16(EE_ADDR_COUNT, cnt);
}

void eepromDumpLast(uint16_t n) {
  uint16_t idx = eeReadU16(EE_ADDR_IDX);
  uint16_t cnt = eeReadU16(EE_ADDR_COUNT);
  if (cnt == 0) {
    Serial.println(F("No logs."));
    return;
  }
  if (n > cnt) n = cnt;
  Serial.print(F("Dumping last ")); Serial.print(n);
  Serial.print(F(" of ")); Serial.print(cnt); Serial.print(F(" entries (cap "));
  Serial.print(MAX_ENTRIES); Serial.println(F("):"));
  int cur = (int)idx - 1;
  if (cur < 0) cur = MAX_ENTRIES - 1;
  for (uint16_t i = 0; i < n; i++) {
    int entryAddr = EE_ADDR_DATA + cur * ENTRY_SIZE;
    LogEntry e;
    EEPROM.get(entryAddr, e);
    bool flame  = e.flags & (1 << 0);
    bool motion = e.flags & (1 << 1);
    bool relay  = e.flags & (1 << 2);
    bool hazard = e.flags & (1 << 3);
    const char* ev =
      (e.code == 1) ? "HAZARD_ON"  :
      (e.code == 2) ? "HAZARD_OFF" :
      (e.code == 3) ? "MANUAL_ON"  :
      (e.code == 4) ? "MANUAL_OFF" : "UNKNOWN";
    Serial.print(F("#")); Serial.print(i + 1);
    Serial.print(F(" t_ms=")); Serial.print(e.ms);
    Serial.print(F(" ev=")); Serial.print(ev);
    Serial.print(F(" gas=")); Serial.print(e.gas);
    Serial.print(F(" flags[flame=")); Serial.print(flame ? "1" : "0");
    Serial.print(F(", motion=")); Serial.print(motion ? "1" : "0");
    Serial.print(F(", relay=")); Serial.print(relay ? "1" : "0");
    Serial.print(F(", hazard=")); Serial.print(hazard ? "1" : "0");
    Serial.println(F("]"));
    cur = (cur - 1);
    if (cur < 0) cur = MAX_ENTRIES - 1;
  }
}

void eepromClear() {
  for (int i = 0; i < EE_BYTES; i++) EEPROM.update(i, 0xFF);
  eeWriteU16(EE_ADDR_IDX, 0);
  eeWriteU16(EE_ADDR_COUNT, 0);
  Serial.println(F("EEPROM logs cleared."));
}

void setup() {
  pinMode(flamePin, INPUT);
  pinMode(motionPin, INPUT);
  pinMode(relayPin, OUTPUT);
  Serial.begin(9600);
  while (!Serial) {}
  eepromInitIfEmpty();
  IrReceiver.begin(irPin, ENABLE_LED_FEEDBACK);
  Serial.print(F("EEPROM ready. max="));
  Serial.print(MAX_ENTRIES);
  Serial.print(F(" entries, count="));
  Serial.println(eeReadU16(EE_ADDR_COUNT));
  Serial.println(F("Send 'D' to dump last 10 logs, 'C' to clear logs."));
}

void loop() {
  int  gasValue       = analogRead(gasPin);
  bool flameDetected  = (digitalRead(flamePin) == HIGH);
  bool motionDetected = (digitalRead(motionPin) == LOW);
  bool hazard         = flameDetected || gasValue > 400 || motionDetected;
  bool relayOn = hazard;
  digitalWrite(relayPin, relayOn ? HIGH : LOW);
  Serial.print("Flame: ");
  Serial.print(flameDetected ? "DETECTED" : "Safe");
  Serial.print(" | Gas: ");
  Serial.print(gasValue);
  Serial.print(" | Motion: ");
  Serial.println(motionDetected ? "DETECTED" : "None");
  if (hazard && (millis() - lastLogTime > 5000)) {
    Serial.println("⚠️ Hazard detected! Relay activated.");
    lastLogTime = millis();
  }
  if (hazard != prevHazard) {
    eepromLog(hazard ? 1 : 2, gasValue, flameDetected, motionDetected, relayOn, hazard);
    prevHazard = hazard;
  }
  if (IrReceiver.decode()) {
    uint32_t raw = IrReceiver.decodedIRData.decodedRawData;
    if (raw == CODE_ON) {
      digitalWrite(relayPin, HIGH);
      Serial.println("Manual ON via IR.");
      eepromLog(3, gasValue, flameDetected, motionDetected, true, hazard);
    } else if (raw == CODE_OFF) {
      digitalWrite(relayPin, LOW);
      Serial.println("Manual OFF via IR.");
      eepromLog(4, gasValue, flameDetected, motionDetected, false, hazard);
    }
    IrReceiver.resume();
  }
  if (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'D' || c == 'd') {
      eepromDumpLast(10);
    } else if (c == 'C' || c == 'c') {
      eepromClear();
    }
  }
  delay(500);
}
