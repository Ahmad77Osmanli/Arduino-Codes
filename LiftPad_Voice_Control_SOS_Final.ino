#include <SoftwareSerial.h>
#include "VoiceRecognitionV3.h"

/*** ==== USER SETTINGS (edit these if needed) ==== ***/
#define WAKE_REC   5   // LiftPad (wake)
#define REC_UP     0
#define REC_DOWN   1
#define REC_STOP   2
#define REC_LIGHTS 3
#define REC_HELP   4   // Help -> will send to NodeMCU

// Relay pins (CW-022 INx)
const int RELAY_UP_PIN     = 4; // Up
const int RELAY_DOWN_PIN   = 5; // Down
const int RELAY_LIGHTS_PIN = 6; // Lights
const int RELAY_SPARE_PIN  = 7; // Spare

// Relay logic (most 4-ch boards are ACTIVE-LOW: LOW = ON, HIGH = OFF)
#define RELAY_ON   LOW
#define RELAY_OFF  HIGH

// Timings
const unsigned long MOTOR_RUN_TIME   = 18000UL; // 18s auto-off (you had 18s)
const unsigned long INTERLOCK_DELAY  = 500UL;   // 0.5s before reversing
const unsigned long LISTEN_WINDOW    = 15000UL; // 15s listening after wake

/*** ==== END USER SETTINGS ==== ***/

VR myVR(2, 3);            // VR: module TX -> D2, module RX -> D3
uint8_t buf[64];

// SoftwareSerial used to talk to NodeMCU
SoftwareSerial espSerial(10, 11); // Arduino D10 (TX) -> NodeMCU RX, D11 (RX) <- NodeMCU TX
const unsigned long ESP_BAUD = 9600;

bool listeningMode = false;
unsigned long listenStartMs = 0;

bool motorRunning = false;
int  activeMotorRelay = -1;
unsigned long motorStartMs = 0;
unsigned long lastMotorStopMs = 0;

void setRelay(int pin, int state) { digitalWrite(pin, state); }
int  getRelay(int pin)            { return digitalRead(pin);   }

// ---------- Motor helpers ----------
void stopMotor() {
  setRelay(RELAY_UP_PIN,   RELAY_OFF);
  setRelay(RELAY_DOWN_PIN, RELAY_OFF);
  motorRunning = false;
  activeMotorRelay = -1;
  lastMotorStopMs = millis();
  Serial.println(F("[MOTOR] STOPPED"));
}

bool canStartMotorNow() {
  return (millis() - lastMotorStopMs >= INTERLOCK_DELAY);
}

void startMotor(int relayPin, const __FlashStringHelper* name) {
  if (!canStartMotorNow()) {
    Serial.println(F("[MOTOR] Interlock: wait ~0.5s before reversing."));
    return;
  }
  stopMotor();
  setRelay(relayPin, RELAY_ON);
  motorRunning = true;
  activeMotorRelay = relayPin;
  motorStartMs = millis();
  Serial.print(F("[MOTOR] ")); Serial.print(name); Serial.println(F(" STARTED"));
}

// ---------- Lights ----------
void toggleLights() {
  int cur = getRelay(RELAY_LIGHTS_PIN);
  int nxt = (cur == RELAY_OFF) ? RELAY_ON : RELAY_OFF;
  setRelay(RELAY_LIGHTS_PIN, nxt);
  Serial.print(F("[LIGHTS] "));
  Serial.println((nxt == RELAY_ON) ? F("ON") : F("OFF"));
}

// ---------- VR load helpers ----------
void loadWakeOnly() {
  if (myVR.clear() == 0) {
    uint8_t set1[1] = { (uint8_t)WAKE_REC };
    int r = myVR.load(set1, 1);
    if (r >= 0) {
      Serial.print(F("[VR] Wake mode active. Say record "));
      Serial.print(WAKE_REC);
      Serial.println(F(" (LiftPad)."));
    } else {
      Serial.println(F("[VR] ERROR: failed to load wake record."));
    }
  } else {
    Serial.println(F("[VR] ERROR: clear() failed in wake mode."));
  }
  listeningMode = false;
}

void loadCommandSet() {
  if (myVR.clear() == 0) {
    uint8_t cmds[5] = { REC_UP, REC_DOWN, REC_STOP, REC_LIGHTS, REC_HELP };
    int r = myVR.load(cmds, 5);
    if (r >= 0) {
      Serial.println(F("[VR] Command mode active (records 0..4)."));
      listeningMode = true;
      listenStartMs = millis();
    } else {
      Serial.println(F("[VR] ERROR: failed to load command set."));
      loadWakeOnly();
    }
  } else {
    Serial.println(F("[VR] ERROR: clear() failed in command mode."));
    loadWakeOnly();
  }
}

// ---------- Command dispatcher ----------
void handleCommand(uint8_t rec) {
  // extend listening window for each valid command
  listenStartMs = millis();

  switch (rec) {
    case REC_UP:
      startMotor(RELAY_UP_PIN,   F("UP"));
      break;

    case REC_DOWN:
      startMotor(RELAY_DOWN_PIN, F("DOWN"));
      break;

    case REC_STOP:
      stopMotor();
      break;

    case REC_LIGHTS:
      toggleLights();
      break;

    case REC_HELP:
      Serial.println(F("[HELP] Assistance requested. Sending to NodeMCU..."));
      // send HELP and small context to NodeMCU (ESP)
      if (espSerial) {
        espSerial.println("HELP|LiftPad");
      }
      break;

    default:
      Serial.print(F("[VR] Unknown record in command mode: "));
      Serial.println(rec);
      break;
  }
}

// ================== Arduino standard ==================
void setup() {
  // Serial for debug
  Serial.begin(115200);
  delay(200);
  Serial.println(F("\n=== LiftPad Voice Controller Boot ==="));

  // VR module UART
  myVR.begin(9600);

  // start espSerial
  espSerial.begin(ESP_BAUD);
  Serial.print(F("[ESP] espSerial started at "));
  Serial.println(ESP_BAUD);

  // Relays: set pins & ensure OFF at boot
  pinMode(RELAY_UP_PIN,     OUTPUT);
  pinMode(RELAY_DOWN_PIN,   OUTPUT);
  pinMode(RELAY_LIGHTS_PIN, OUTPUT);
  pinMode(RELAY_SPARE_PIN,  OUTPUT);

  setRelay(RELAY_UP_PIN,     RELAY_OFF);
  setRelay(RELAY_DOWN_PIN,   RELAY_OFF);
  setRelay(RELAY_LIGHTS_PIN, RELAY_OFF);
  setRelay(RELAY_SPARE_PIN,  RELAY_OFF);

  // Enter wake mode (only the wake record is active)
  loadWakeOnly();

  Serial.println(F("[READY] Waiting for wake word..."));
}

void loop() {
  // Check voice recognition
  int ret = myVR.recognize(buf, 50);  // short poll
  if (ret > 0) {
    uint8_t rec = buf[1];
    Serial.print(F("[VR] Heard record: ")); Serial.println(rec);

    if (!listeningMode) {
      // Wake mode: only WAKE_REC should be active
      if (rec == WAKE_REC) {
        Serial.println(F("[VR] Wake word detected (LiftPad)."));
        loadCommandSet(); // start LISTEN_WINDOW
      } else {
        // Unexpected number while in wake mode; reload wake just in case
        loadWakeOnly();
      }
    } else {
      // In command mode: handle, and keep window alive
      handleCommand(rec);
    }

    // Small debounce so a single utterance doesn't double-trigger
    delay(200);
  }

  // Motor auto-stop
  if (motorRunning && (millis() - motorStartMs >= MOTOR_RUN_TIME)) {
    stopMotor();
    Serial.println(F("[MOTOR] Auto-stopped after time limit."));
  }

  // Command window timeout → back to wake mode
  if (listeningMode && (millis() - listenStartMs >= LISTEN_WINDOW)) {
    Serial.println(F("[VR] Listen window expired. Returning to wake mode."));
    loadWakeOnly();
  }

  // Optional: print any response from ESP (ACK)
  if (espSerial.available()) {
    String s = espSerial.readStringUntil('\n');
    s.trim();
    if (s.length()) {
      Serial.print(F("[ESP RX] "));
      Serial.println(s);
    }
  }
}
