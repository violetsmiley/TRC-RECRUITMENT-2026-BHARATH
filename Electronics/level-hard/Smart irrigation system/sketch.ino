#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#define SOIL_PIN 34       
#define RELAY_PIN 25      
#define STATUS_LED 2      
LiquidCrystal_I2C lcd(0x27, 20, 4);
const char* ssid = "Wokwi-GUEST";
const char* password = "";
const char* weatherUrl = "https://api.open-meteo.com/v1/forecast?latitude=51.5074&longitude=-0.1278&hourly=precipitation_probability&current_weather=true";
bool rainPredicted = false;
bool pumpActive = false;
unsigned long pumpStartTime = 0;
unsigned long totalPumpTimeMs = 0;
unsigned long lastWeatherCheck = 0;
const unsigned long weatherInterval = 60000;
const float FLOW_RATE_LPS = 0.02;
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Irrigation");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");
  WiFi.begin(ssid, password);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  lcd.clear();
  checkWeather();
}
void loop() {
  int rawAnalog = analogRead(SOIL_PIN);
  float moisture = map(rawAnalog, 0, 4095, 0, 100);
  if (millis() - lastWeatherCheck > weatherInterval || lastWeatherCheck == 0) {
    checkWeather();
    lastWeatherCheck = millis();
  }
  bool dry = moisture < 30.0;
  bool optimal = (moisture >= 30.0 && moisture <= 60.0);
  bool wet = moisture > 60.0;
  if (dry && !rainPredicted) {
    if (!pumpActive) {
      pumpActive = true;
      pumpStartTime = millis();
      digitalWrite(RELAY_PIN, HIGH);
    }
  } else {
    if (pumpActive) {
      totalPumpTimeMs += (millis() - pumpStartTime);
      pumpActive = false;
      digitalWrite(RELAY_PIN, LOW);
    }
  }
  unsigned long currentSession = pumpActive ? (millis() - pumpStartTime) : 0;
  float totalWaterUsedLiters = ((totalPumpTimeMs + currentSession) / 1000.0) * FLOW_RATE_LPS;
  bool emergencyAlert = (pumpActive && currentSession > 15000 && dry);
  updateDisplay(moisture, pumpActive, rainPredicted, totalWaterUsedLiters, emergencyAlert);
  delay(1000);
}
void checkWeather() {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(weatherUrl);
    int httpCode = http.GET();
    if (httpCode == 200) {
      String payload = http.getString();
      DynamicJsonDocument doc(4096);
      deserializeJson(doc, payload);
      rainPredicted = false;
      for (int i = 0; i < 3; i++) {
        int prob = doc["hourly"]["precipitation_probability"][i];
        if (prob >= 50) {
          rainPredicted = true;
          break;
        }
      }
      Serial.printf("Weather checked. Rain predicted next 3h: %s\n", rainPredicted ? "YES" : "NO");
    } else {
      Serial.println("Weather API request failed, fallback: No Rain.");
      rainPredicted = false;
    }
    http.end();
  }
}

void updateDisplay(float moisture, bool pump, bool rain, float waterUsed, bool emergency) {
  lcd.clear();

  if (emergency) {
    lcd.setCursor(0, 0);
    lcd.print("! SYSTEM ALERT !");
    lcd.setCursor(0, 1);
    lcd.print("Water Not Reaching");
    lcd.setCursor(0, 2);
    lcd.print("Soil - Check Sys!");
    return;
  }
  lcd.setCursor(0, 0);
  lcd.print("Soil: ");
  lcd.print((int)moisture);
  lcd.print("% ");
  if (moisture < 30) lcd.print("[Dry]");
  else if (moisture <= 60) lcd.print("[Opt]");
  else lcd.print("[Wet]");
  lcd.setCursor(0, 1);
  lcd.print("Rain 3h: ");
  lcd.print(rain ? "YES (Hold)" : "NO");
  lcd.setCursor(0, 2);
  lcd.print("Pump: ");
  lcd.print(pump ? "RUNNING (ON)" : "OFF");
  lcd.setCursor(0, 3);
  lcd.print("Used: ");
  lcd.print(waterUsed, 2);
  lcd.print(" L");
  Serial.printf("Moisture: %.1f%% | Pump: %s | Rain: %s | Water: %.2fL\n",
                moisture, pump ? "ON" : "OFF", rain ? "YES" : "NO", waterUsed);
}
