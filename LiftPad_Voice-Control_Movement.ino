/* Robust LiftPad — continuous voice controller (no wake).
   - Keeps Up/Down/Stop for winch (relays), adds Forward/Back/Left/Right for drive (L298N).
   - If loading the full command set fails, the sketch probes each record and loads only those present.
   - Interlock + safety auto-stop retained.
*/

#include "VoiceRecognitionV3.h"

/*** RECORD IDS - change if you trained different slots ***/
#define REC_UP        0
#define REC_DOWN      1
#define REC_STOP      2
#define REC_FORWARD   3
#define REC_BACKWARD  4
#define REC_LEFT      5
#define REC_RIGHT     6

/**** PINS ****/
// Winch relays (CW-022) - active LOW (LOW = ON)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH
const int RELAY_UP_PIN   = 4;
const int RELAY_DOWN_PIN = 5;

// L298N / HW-095 drive pins
const int IN1_pin = 6; // left forward
const int IN2_pin = 7; // left backward
const int IN3_pin = 8; // right forward
const int IN4_pin = 9; // right backward

/**** TIMINGS & SAFETY ****/
const unsigned long WINCH_RUN_TIME   = 20000UL; // 20s auto-stop for winch
const unsigned long DRIVE_RUN_TIME   = 60000UL; // 60s auto-stop for drive
const unsigned long INTERLOCK_DELAY  = 500UL;   // 0.5s before reversing
const unsigned long DEBOUNCE_MS      = 150UL;   // avoid double triggers

/**** VR MODULE (module TX -> D2, RX -> D3) ****/
VR myVR(2, 3);
uint8_t buf[64];

/**** RUNTIME STATE ****/
// winch
bool winchRunning = false;
int  winchActivePin = -1; // RELAY_UP_PIN or RELAY_DOWN_PIN
unsigned long winchStartMs = 0;
unsigned long lastWinchStopMs = 0;

// drive
bool driveRunning = false;
int  driveAction = -1; // 0=FWD,1=BACK,2=LEFT,3=RIGHT
unsigned long driveStartMs = 0;
unsigned long lastDriveStopMs = 0;

// available records list (populated at startup)
bool recordAvailable[7] = { false, false, false, false, false, false, false };

/**** HELPERS: winch ****/
inline void setRelay(int pin, int state) { digitalWrite(pin, state); }

bool canStartWinchNow() {
  return (millis() - lastWinchStopMs) >= INTERLOCK_DELAY;
}

void stopWinch() {
  setRelay(RELAY_UP_PIN, HIGH);   // RELAY_OFF
  setRelay(RELAY_DOWN_PIN, HIGH); // RELAY_OFF
  winchRunning = false;
  winchActivePin = -1;
  lastWinchStopMs = millis();
  Serial.println(F("[WINCH] STOPPED"));
}

void startWinch(int relayPin, const __FlashStringHelper* name) {
  if (!canStartWinchNow()) {
    Serial.println(F("[WINCH] Interlock: wait 0.5s before reversing."));
    return;
  }

  // stop drive before starting winch
  if (driveRunning) {
    digitalWrite(IN1_pin, LOW); digitalWrite(IN2_pin, LOW);
    digitalWrite(IN3_pin, LOW); digitalWrite(IN4_pin, LOW);
    driveRunning = false;
    driveAction = -1;
    lastDriveStopMs = millis();
    Serial.println(F("[DRIVE] Stopped (auto) before winch start"));
  }

  // stop current winch state then start requested
  stopWinch();
  if (relayPin == RELAY_UP_PIN) {
    setRelay(RELAY_UP_PIN, LOW);   // ON
    setRelay(RELAY_DOWN_PIN, HIGH);
  } else {
    setRelay(RELAY_UP_PIN, HIGH);
    setRelay(RELAY_DOWN_PIN, LOW);
  }
  winchRunning = true;
  winchActivePin = relayPin;
  winchStartMs = millis();
  Serial.print(F("[WINCH] ")); Serial.print(name); Serial.println(F(" STARTED"));
}

/**** HELPERS: drive ****/
bool canStartDriveNow() {
  return (millis() - lastDriveStopMs) >= INTERLOCK_DELAY;
}

void motorsStopAll() {
  digitalWrite(IN1_pin, LOW);
  digitalWrite(IN2_pin, LOW);
  digitalWrite(IN3_pin, LOW);
  digitalWrite(IN4_pin, LOW);
  driveRunning = false;
  driveAction = -1;
  lastDriveStopMs = millis();
  Serial.println(F("[DRIVE] STOPPED"));
}

void startDrive(int action) {
  if (!canStartDriveNow()) {
    Serial.println(F("[DRIVE] Interlock: wait 0.5s before reversing/starting."));
    return;
  }

  // stop winch before driving
  if (winchRunning) {
    setRelay(RELAY_UP_PIN, HIGH);
    setRelay(RELAY_DOWN_PIN, HIGH);
    winchRunning = false;
    winchActivePin = -1;
    lastWinchStopMs = millis();
    Serial.println(F("[WINCH] Stopped (auto) before drive start"));
  }

  // ensure all outputs are idle then start requested action
  motorsStopAll();

  switch (action) {
    case 0: // FORWARD
      digitalWrite(IN1_pin, HIGH); digitalWrite(IN2_pin, LOW);
      digitalWrite(IN3_pin, HIGH); digitalWrite(IN4_pin, LOW);
      Serial.println(F("[DRIVE] FORWARD started"));
      break;

    case 1: // BACKWARD
      digitalWrite(IN1_pin, LOW); digitalWrite(IN2_pin, HIGH);
      digitalWrite(IN3_pin, LOW); digitalWrite(IN4_pin, HIGH);
      Serial.println(F("[DRIVE] BACKWARD started"));
      break;

    case 2: // LEFT (spin left)
      digitalWrite(IN1_pin, LOW); digitalWrite(IN2_pin, HIGH); // left back
      digitalWrite(IN3_pin, HIGH); digitalWrite(IN4_pin, LOW); // right fwd
      Serial.println(F("[DRIVE] LEFT started"));
      break;

    case 3: // RIGHT (spin right)
      digitalWrite(IN1_pin, HIGH); digitalWrite(IN2_pin, LOW); // left fwd
      digitalWrite(IN3_pin, LOW); digitalWrite(IN4_pin, HIGH); // right back
      Serial.println(F("[DRIVE] RIGHT started"));
      break;

    default:
      Serial.println(F("[DRIVE] Unknown action request"));
      return;
  }

  driveRunning = true;
  driveAction = action;
  driveStartMs = millis();
}

/**** VR utilities ****/
bool vrClearWithRetry(int attempts = 4, unsigned long delayMs = 250) {
  while (attempts--) {
    if (myVR.clear() == 0) return true;
    delay(delayMs);
  }
  return false;
}

// Try to load the given array of record IDs; return true if success
bool tryLoadRecords(uint8_t *recs, uint8_t len) {
  if (!vrClearWithRetry()) {
    Serial.println(F("[VR] clear() failed (cannot prepare module)"));
    return false;
  }
  int r = myVR.load(recs, len);
  if (r >= 0) {
    Serial.print(F("[VR] load() OK — loaded "));
    Serial.print(r);
    Serial.println(F(" records."));
    return true;
  } else {
    Serial.print(F("[VR] load() failed (retval="));
    Serial.print(r);
    Serial.println(F(")"));
    return false;
  }
}

// Probe each record individually to see which records exist on the module.
// Fills recordAvailable[] and returns count.
int probeRecordsAndPrepare() {
  const uint8_t allRecs[7] = { REC_UP, REC_DOWN, REC_STOP, REC_FORWARD, REC_BACKWARD, REC_LEFT, REC_RIGHT };
  uint8_t foundList[7];
  int foundCount = 0;

  Serial.println(F("[VR] Probing individual records..."));
  for (int i = 0; i < 7; ++i) {
    uint8_t id = allRecs[i];
    // try to load single record id into module (load returns >=0 if success)
    if (vrClearWithRetry(3, 150)) {
      int r = myVR.load(&id, 1);
      if (r >= 0) {
        foundList[foundCount++] = id;
        Serial.print(F("[VR] Record "));
        Serial.print(id);
        Serial.println(F(" => OK (present)."));
      } else {
        Serial.print(F("[VR] Record "));
        Serial.print(id);
        Serial.println(F(" => MISSING (or load failed)."));
      }
      delay(80);
    } else {
      Serial.println(F("[VR] clear() failed while probing."));
      break;
    }
  }

  // Now if we found any records, load them all together so module listens to the available set
  if (foundCount > 0) {
    if (tryLoadRecords(foundList, foundCount)) {
      // mark available
      for (int i = 0; i < foundCount; ++i) {
        uint8_t id = foundList[i];
        if (id <= 6) recordAvailable[id] = true;
      }
      Serial.print(F("[VR] Final active records count: "));
      Serial.println(foundCount);
      return foundCount;
    } else {
      Serial.println(F("[VR] Failed to load found records as a set."));
      return 0;
    }
  }

  Serial.println(F("[VR] No records found during probe."));
  return 0;
}

/**** Command dispatcher ****/
void handleCommand(uint8_t rec) {
  // Ignore records that weren't verified at startup (defensive check)
  if (rec <= 6 && !recordAvailable[rec]) {
    Serial.print(F("[VR] Record "));
    Serial.print(rec);
    Serial.println(F(" heard but not available (not trained)."));
    return;
  }

  switch (rec) {
    case REC_UP:
      startWinch(RELAY_UP_PIN, F("UP"));
      break;
    case REC_DOWN:
      startWinch(RELAY_DOWN_PIN, F("DOWN"));
      break;
    case REC_STOP:
      stopWinch();
      motorsStopAll();
      break;
    case REC_FORWARD:
      startDrive(0);
      break;
    case REC_BACKWARD:
      startDrive(1);
      break;
    case REC_LEFT:
      startDrive(2);
      break;
    case REC_RIGHT:
      startDrive(3);
      break;
    default:
      Serial.print(F("[VR] Unknown record in handler: "));
      Serial.println(rec);
      break;
  }
}

/**** Setup & main loop ****/
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== LiftPad Voice Controller (no wake) Boot ==="));

  // Initialize outputs BEFORE VR to avoid floating pulses at boot
  pinMode(RELAY_UP_PIN, OUTPUT);
  pinMode(RELAY_DOWN_PIN, OUTPUT);
  setRelay(RELAY_UP_PIN, RELAY_OFF);
  setRelay(RELAY_DOWN_PIN, RELAY_OFF);

  pinMode(IN1_pin, OUTPUT);
  pinMode(IN2_pin, OUTPUT);
  pinMode(IN3_pin, OUTPUT);
  pinMode(IN4_pin, OUTPUT);
  motorsStopAll(); // ensure drive outputs idle (LOW)

  // start VR
  myVR.begin(9600);
  delay(300); // let the module settle

  // First try to load the entire expected command set at once
  uint8_t allCmds[7] = { REC_UP, REC_DOWN, REC_STOP, REC_FORWARD, REC_BACKWARD, REC_LEFT, REC_RIGHT };
  Serial.println(F("[VR] Attempting to load full command set (0..6)"));
  if (tryLoadRecords(allCmds, 7)) {
    // mark all available
    for (int i = 0; i < 7; ++i) recordAvailable[i] = true;
    Serial.println(F("[VR] All commands loaded successfully."));
  } else {
    // full load failed — probe each record and load available ones
    int found = probeRecordsAndPrepare();
    if (found == 0) {
      Serial.println(F("[VR] ****************************************"));
      Serial.println(F("[VR] ERROR: No command records available."));
      Serial.println(F("[VR] Please run the Elechouse training sketch and record:"));
      Serial.println(F("  0: Up"));
      Serial.println(F("  1: Down"));
      Serial.println(F("  2: Stop"));
      Serial.println(F("  3: Forward"));
      Serial.println(F("  4: Backward"));
      Serial.println(F("  5: Left"));
      Serial.println(F("  6: Right"));
      Serial.println(F("[VR] After training, re-upload or reset this sketch."));
      Serial.println(F("[VR] ****************************************"));
      // Continue but no records -> nothing to act on until trained
    } else {
      Serial.print(F("[VR] Loaded "));
      Serial.print(found);
      Serial.println(F(" records (partial)."));
    }
  }

  Serial.println(F("[READY] Listening for commands (if records present)."));
}

void loop() {
  int ret = myVR.recognize(buf, 50); // poll VR
  if (ret > 0) {
    uint8_t rec = buf[1];
    Serial.print(F("[VR] Heard record: "));
    Serial.println(rec);
    handleCommand(rec);
    delay(DEBOUNCE_MS); // debounce so one utterance doesn't double trigger
  }

  // winch auto-stop
  if (winchRunning && (millis() - winchStartMs >= WINCH_RUN_TIME)) {
    stopWinch();
    Serial.println(F("[WINCH] Auto-stopped after timeout"));
  }

  // drive auto-stop
  if (driveRunning && (millis() - driveStartMs >= DRIVE_RUN_TIME)) {
    motorsStopAll();
    Serial.println(F("[DRIVE] Auto-stopped after timeout"));
  }
}
