#include <TinyGPS++.h>
#include <SoftwareSerial.h>

static const int RXPin = D4, TXPin = D3; 
static const uint32_t GPSBaud = 9600;

// The TinyGPS++ object
TinyGPSPlus gps;

// SoftwareSerial for GPS
SoftwareSerial gpsSerial(RXPin, TXPin);

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(GPSBaud);

  Serial.println("=== Neo-6M GPS Cordinates ===");
}

void loop() {
  // Read from GPS module
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());

    if (gps.location.isUpdated()) {
      Serial.print("Latitude: ");
      Serial.println(gps.location.lat(), 6);
      Serial.print("Longitude: ");
      Serial.println(gps.location.lng(), 6);
      Serial.print("Satellites: ");
      Serial.println(gps.satellites.value());
      Serial.print("HDOP: ");
      Serial.println(gps.hdop.value());
      Serial.println("-----");
    }
  }
}
