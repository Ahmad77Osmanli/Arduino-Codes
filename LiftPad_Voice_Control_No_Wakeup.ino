#include "VoiceRecognitionV3.h"

/*** USER SETTINGS ***/
#define WAKE_REC   5   // LiftPad (unused now)
#define REC_UP     0
#define REC_DOWN   1
#define REC_STOP   2
#define REC_HELP   3   // HELP triggers ESP

// Relay pins
const int RELAY_UP_PIN     = 4;
const int RELAY_DOWN_PIN   = 5;
const int ESP_SIGNAL_PIN   = 8; // signal pulse to ESP

// Relay logic
#define RELAY_ON   LOW       // motors active LOW
#define RELAY_OFF  HIGH
#define ESP_TRIGGER HIGH
#define ESP_IDLE    LOW

// Timings
const unsigned long MOTOR_RUN_TIME   = 18000UL; // 15s
const unsigned long INTERLOCK_DELAY  = 500UL;   // 0.5s
const unsigned long LISTEN_WINDOW    = 10000UL; // kept but not used to revert to wake

/**** END USER SETTINGS ****/

VR myVR(2,3); // VR module (module TX -> Arduino D2, module RX -> Arduino D3)
uint8_t buf[64];

bool listeningMode = true; // always in command mode now
unsigned long listenStartMs = 0;

bool motorRunning = false;
int activeMotorRelay = -1;
unsigned long motorStartMs = 0;
unsigned long lastMotorStopMs = 0;

// ---------- Helpers ----------
void setRelay(int pin, int state){ digitalWrite(pin,state); }
int getRelay(int pin){ return digitalRead(pin); }

void stopMotor(){
  setRelay(RELAY_UP_PIN, RELAY_OFF);
  setRelay(RELAY_DOWN_PIN, RELAY_OFF);
  motorRunning=false;
  activeMotorRelay=-1;
  lastMotorStopMs = millis();
  Serial.println(F("[MOTOR] STOPPED"));
}

bool canStartMotorNow(){ return (millis()-lastMotorStopMs)>=INTERLOCK_DELAY; }

void startMotor(int relayPin, const __FlashStringHelper* name){
  if(!canStartMotorNow()){ Serial.println(F("[MOTOR] Interlock, wait 0.5s")); return; }
  stopMotor();
  setRelay(relayPin, RELAY_ON);
  motorRunning=true;
  activeMotorRelay=relayPin;
  motorStartMs=millis();
  Serial.print(F("[MOTOR] ")); Serial.print(name); Serial.println(F(" STARTED"));
}

bool vrClearWithRetry(int attempts=3){
  while(attempts--){
    if(myVR.clear()==0) return true;
    delay(200);
  }
  return false;
}

void loadCommandSet(){
  if(vrClearWithRetry()){
    uint8_t cmds[4]={ REC_UP, REC_DOWN, REC_STOP, REC_HELP };
    if(myVR.load(cmds,4)>=0){
      listeningMode=true;
      listenStartMs=millis();
      Serial.println(F("[VR] Command mode active (records 0..3). Listening continuously."));
    } else {
      Serial.println(F("[VR] ERROR: failed to load command set."));
    }
  } else {
    Serial.println(F("[VR] ERROR: clear() failed when loading command set."));
  }
}

void handleCommand(uint8_t rec){
  // When in continuous mode we still refresh listenStartMs (keeps existing behavior)
  listenStartMs=millis();

  switch(rec){
    case REC_UP:
      startMotor(RELAY_UP_PIN,F("UP"));
      break;
    case REC_DOWN:
      startMotor(RELAY_DOWN_PIN,F("DOWN"));
      break;
    case REC_STOP:
      stopMotor();
      break;
    case REC_HELP:
      Serial.println(F("[HELP] Triggering ESP"));
      digitalWrite(ESP_SIGNAL_PIN, ESP_TRIGGER);
      delay(500); // 0.5s pulse
      digitalWrite(ESP_SIGNAL_PIN, ESP_IDLE);
      break;
    default:
      Serial.print(F("[VR] Unknown record: ")); Serial.println(rec);
      break;
  }
}

// ================= Arduino =================
void setup(){
  Serial.begin(115200);
  delay(300);
  Serial.println(F("=== LiftPad Voice Controller Boot (NO WAKE MODE) ==="));

  myVR.begin(9600);
  delay(300); // give VR module time to be ready

  // Initialize relay/signal pins immediately so they don't float at boot
  pinMode(RELAY_UP_PIN, OUTPUT);
  pinMode(RELAY_DOWN_PIN, OUTPUT);
  pinMode(ESP_SIGNAL_PIN, OUTPUT);

  setRelay(RELAY_UP_PIN, RELAY_OFF);
  setRelay(RELAY_DOWN_PIN, RELAY_OFF);
  setRelay(ESP_SIGNAL_PIN, ESP_IDLE);

  // Load command set so VR listens for commands continuously
  loadCommandSet();

  Serial.println(F("[READY] Listening for commands (Up/Down/Stop/Help)..."));
}

void loop(){
  int ret = myVR.recognize(buf,50);  // poll recognizer
  if(ret > 0){
    uint8_t rec = buf[1];
    Serial.print(F("[VR] Heard record: ")); Serial.println(rec);

    // Directly handle any recognized record (no wake mode)
    handleCommand(rec);

    // small debounce so a single utterance doesn't double-trigger
    delay(150);
  }

  // motor auto-stop
  if(motorRunning && (millis() - motorStartMs >= MOTOR_RUN_TIME)){
    stopMotor();
    Serial.println(F("[MOTOR] Auto-stopped after 15s"));
  }

  // NOTE: we intentionally do NOT revert to a wake-only mode.
  // The LISTEN_WINDOW constant remains for compatibility but is not used to switch modes.
}
