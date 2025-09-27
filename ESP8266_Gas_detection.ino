#define GAS_SENSOR_AO A0    // Analog pin
#define LED_PIN       12    // D6
#define BUZZER_PIN    13    // D7

void setup() {
  Serial.begin(115200);      // Serial monitoru işə sal
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
}

void loop() {
  int gasValue = analogRead(GAS_SENSOR_AO);  // Analog dəyəri oxu (0 - 1023 arası)

  Serial.print("Qaz dəyəri: ");
  Serial.println(gasValue);   // Serial monitorda yazdır

  // Təhlükə üçün sadə şərt
  if (gasValue > 200) {       // 500-dən yuxarı dəyərlərdə təhlükə
    digitalWrite(LED_PIN, HIGH);    
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
  }

  delay(500);  // yarım saniyəlik gecikmə
}
