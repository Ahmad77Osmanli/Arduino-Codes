#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>

// ===== WiFi Settings =====
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// ===== Telegram Bot Settings =====
String botToken = "YOUR_BOT_TOKEN";
String chatID   = "YOUR_CHAT_ID";

// ===== Behavior =====
const unsigned long HELP_COOLDOWN = 60000UL; // 60s cooldown to avoid spamming
unsigned long lastHelpSent = 0;

void setup() {
  Serial.begin(9600); // match Arduino espSerial baud
  delay(50);
  Serial.println();
  Serial.println("[ESP] Booting...");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("[WiFi] Connecting");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
    if (millis() - start > 20000) {
      Serial.println();
      Serial.println("[WiFi] Timeout connecting. Restarting in 5s...");
      delay(5000);
      ESP.restart();
    }
  }
  Serial.println();
  Serial.print("[WiFi] Connected. IP=");
  Serial.println(WiFi.localIP());
  Serial.println("[ESP] Waiting for HELP on Serial...");
}

void sendTelegramMessage(const String &msg) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[TG] WiFi not connected, cannot send.");
    return;
  }

  String url = "https://api.telegram.org/bot" + botToken + "/sendMessage";
  WiFiClientSecure client;
  client.setInsecure(); // prototype: skip cert validation
  HTTPClient https;

  String payload = "{\"chat_id\":\"" + chatID + "\",\"text\":\"" + msg + "\"}";

  Serial.println("[TG] POST " + url);
  Serial.print("[TG] payload: ");
  Serial.println(payload);

  if (https.begin(client, url)) {
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.POST(payload);
    if (httpCode > 0) {
      Serial.print("[TG] HTTP code: ");
      Serial.println(httpCode);
      String resp = https.getString();
      Serial.print("[TG] resp: ");
      Serial.println(resp);
      // send ACK back to Arduino
      Serial.println("SENT|OK");
    } else {
      Serial.print("[TG] POST failed: ");
      Serial.println(https.errorToString(httpCode));
      Serial.println("SENT|ERR");
    }
    https.end();
  } else {
    Serial.println("[TG] begin() failed");
    Serial.println("SENT|ERR");
  }
}

void loop() {
  // Read incoming line from Arduino (terminated by '\n')
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) return;
    Serial.print("[RX] ");
    Serial.println(line);

    // Expected: "HELP" or "HELP|LiftPad"
    if (line.startsWith("HELP")) {
      unsigned long now = millis();
      if (now - lastHelpSent < HELP_COOLDOWN) {
        Serial.println("[RX] HELP ignored due to cooldown.");
        Serial.println("SENT|COOLDOWN");
        return;
      }
      lastHelpSent = now;

      // Optional: append device/IP info
      String extra = "";
      int p = line.indexOf('|');
      if (p >= 0) extra = line.substring(p + 1);

      String msg = "🚨 EMERGENCY: Help requested";
      if (extra.length()) msg += " (" + extra + ")";
      msg += "\\nDevice: LiftPad prototype";
      msg += "\\nIP: " + WiFi.localIP().toString();

      sendTelegramMessage(msg);
    } else {
      Serial.println("[RX] Unknown serial message; ignoring.");
    }
  }

  // keep WiFi alive (simple)
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(1000);
  }
}

