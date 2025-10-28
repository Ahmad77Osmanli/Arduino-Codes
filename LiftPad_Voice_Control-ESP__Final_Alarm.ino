#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>

const char* WIFI_SSID = "Ahmad";
const char* WIFI_PASSWORD = "ahmad@20117";

// ===== Telegram Bot Settings =====
String BOT_TOKEN = "8393506584:AAHSIbm0QtC8OnWK3AMhamsTTXOjki3XZEs";
String CHAT_ID   = "6507966671"; 
String Message = "EMERGENCY ALERT — HELP NEEDED                                                  LiftPad Unit: 0000001                                                                                Coordinates: 40.41924 N, 49.91664 E                                                                 Maps: https://maps.google.com/?q=40.41924,49.91664                        The Help command has been activated. Immediate assistance required.";

// Pins to monitor
const int HELP_PIN  = D1; 
const int EVENT_PIN = D2; 
const int ALARM_PIN = D3; 

// Alarm duration and debounce
const unsigned long ALARM_DURATION_MS = 20000UL; 
const unsigned long HELP_DEBOUNCE_MS  = 50UL;    // debounce for the HELP button

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// state
int lastHelpState = LOW;
unsigned long lastHelpChangeMs = 0;

// alarm state
bool alarmActive = false;
unsigned long alarmStartMs = 0;

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(250);
    Serial.print(".");
    // keep loop alive, but also avoid infinite blocking too long
    if (millis() - start > 30000) { // 30s timeout
      Serial.println("\nWiFi connect timeout - retrying");
      start = millis();
    }
  }
  Serial.println("\nWiFi connected");
}

void sendTelegram(const String &msg){
  Serial.println("[ESP] Sending message: " + msg);
  // ensure WiFi is connected
  connectWiFi();
  client.setInsecure(); // allow self-signed certs (keeps your prior behavior)
  bool ok = bot.sendMessage(CHAT_ID, msg, "");
  Serial.println(ok ? "[ESP] sendMessage OK" : "[ESP] sendMessage FAILED");
}

void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(HELP_PIN, INPUT);        
  pinMode(EVENT_PIN, INPUT);       
  pinMode(ALARM_PIN, OUTPUT);
  digitalWrite(ALARM_PIN, LOW);    // ensure alarm is off at boot

  connectWiFi();
  Serial.println("[ESP] Ready, monitoring HELP_PIN.");
}

void loop() {
  // read help pin
  int helpState = digitalRead(HELP_PIN);

  // simple debounce on rising edge
  if (helpState != lastHelpState) {
    lastHelpChangeMs = millis();
  }
  if ((millis() - lastHelpChangeMs) > HELP_DEBOUNCE_MS) {
    // stable state for debounce window
    static bool lastStableHelp = LOW;
    if (helpState == HIGH && lastStableHelp == LOW) {
      // rising edge detected -> send message once for this press

      // start alarm period (non-blocking)
      digitalWrite(ALARM_PIN, HIGH);
      alarmActive = true;
      alarmStartMs = millis();
      sendTelegram(Message);

      Serial.println("[ALARM] Activated for 60s");
    }
    lastStableHelp = helpState;
  }

  lastHelpState = helpState;

  // handle alarm timeout (non-blocking)
  if (alarmActive) {
    if (millis() - alarmStartMs >= ALARM_DURATION_MS) {
      digitalWrite(ALARM_PIN, LOW);
      alarmActive = false;
      Serial.println("[ALARM] Deactivated after timeout");
    }
  }

  // small yield to keep WiFi stack happy and reduce CPU usage
  delay(10);
}

