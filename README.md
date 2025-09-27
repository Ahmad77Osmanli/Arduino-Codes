# Arduino-Codes
WRO 2025 Future innovators Junior Project Arduino and ESP8266 NodeMCU code
# LiftPad Voice-Controlled Relay & Telegram Alert

**Author:** Ahmad Osmanli  
**Event:** WRO 2025 Future Innovators  
**License:** MIT License  

---

## Project Overview

This project demonstrates a **voice-controlled relay system** using **Arduino Uno**, **Elechouse Voice Recognition Module V3.1**, and a **CW-022 4-channel relay module**, integrated with a **NodeMCU ESP8266** to send **personal Telegram messages** when help is requested.  

The system features:  
- **Wake word detection** (`LiftPad`) to activate command mode  
- **Voice commands** to control motors (up/down/stop) and lights  
- **Automatic motor stop** after 15 seconds for safety  
- **Help command** triggers Telegram notifications  
- **Telegram integration** via ESP8266 Wi-Fi  

---

## Hardware Required

- Arduino Uno  
- NodeMCU ESP8266 (V3 with CH340G driver)  
- Elechouse Voice Recognition V3.1 module  
- CW-022 4-channel relay module  
- Motors or devices connected to relays (Up / Down / Lights)  
- Common USB power or regulated 5V supply  
- Jumper wires  

---

## Wiring Overview

**Arduino Uno ↔ NodeMCU (direct Serial, shared GND)**  

| Arduino Pin | NodeMCU Pin | Function                  |
|-------------|------------|--------------------------|
| D10 (TX)    | RX         | Arduino → NodeMCU data   |
| D11 (RX)    | TX         | NodeMCU → Arduino data   |
| GND         | GND        | Common ground             |

**Relays** (CW-022) connected to Arduino:  

| Relay Pin | Function  | Logic           |
|-----------|-----------|----------------|
| 4         | Up        | LOW = ON        |
| 5         | Down      | LOW = ON        |
| 6         | Lights    | LOW = ON        |
| 7         | Spare     | Optional        |

**Voice Recognition Module V3.1**  

| VR Pin | Arduino Pin | Function |
|--------|------------|---------|
| TX     | D2         | VR → Arduino |
| RX     | D3         | Arduino → VR |

> ⚠️ Ensure NodeMCU RX is not directly exposed to 5V from Arduino in long-term deployment. For prototypes, it can work directly, but using a level shifter or voltage divider is safer.

---

## Software Setup

1. **Arduino UNO Sketch**  
   - Upload the Arduino sketch (`LiftPad_Arduino.ino`)  
   - Make sure SoftwareSerial pins match wiring (D10/D11)  

2. **NodeMCU ESP8266 Sketch**  
   - Replace the placeholders in `LiftPad_ESP.ino`:  
     ```cpp
     const char* ssid     = "YOUR_WIFI_SSID";
     const char* password = "YOUR_WIFI_PASSWORD";
     String botToken = "YOUR_BOT_TOKEN";
     String chatID   = "YOUR_CHAT_ID";
     ```  
   - Upload NodeMCU sketch via USB.  

3. **Voice Recognition Training**  
   - Train commands:  
     - Record 0 → "Up"  
     - Record 1 → "Down"  
     - Record 2 → "Stop"  
     - Record 3 → "Lights"  
     - Record 4 → "Help"  
     - Record 5 → "LiftPad" (wake word)  
   - Test recognition using Serial Monitor  

4. **Serial Monitor Settings**  
   - Arduino UNO: 115200 baud  
   - NodeMCU: 9600 baud  

---

## Usage

1. Power the Arduino and NodeMCU.  
2. Say **"LiftPad"** to activate command mode (10–15 sec window).  
3. Speak one of the trained commands:  
   - **Up / Down / Stop / Lights / Help**  
4. If **Help** is triggered, a **Telegram message** is sent to your personal chat.  
5. System returns to wake mode after the listening window expires.

---

## Notes & Tips

- Motors are automatically stopped after 15s to prevent damage.  
- Only one motor direction can run at a time (interlock delay included).  
- Telegram bot requires a valid **bot token** and your **chat ID**.  
- For testing, you can press **LiftPad wake word** and then say **Help**.  
- Include `LICENSE` file (MIT) if redistributing code.  

---

## Acknowledgments

- **Elechouse Voice Recognition Module V3.1**  
- **WRO 2025 Future Innovators**  
- Open-source Arduino & ESP8266 libraries
- For Website's code please visit https://github.com/Ahmad77Osmanli/LiftPadWebsite

---

## GitHub Repository

Include your code files:  


