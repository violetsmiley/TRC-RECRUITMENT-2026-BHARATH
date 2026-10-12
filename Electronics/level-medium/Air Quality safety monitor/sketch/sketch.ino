#include "DHT.h"
#define DHTPIN 23          
#define MQ135PIN 34
#define LEDBUZZERPIN 18
#define DHTTYPE DHT22
const int GASTHRESHOLD = 1500;
const float HUMIDITYTHRESHOLD = 70.0;
DHT dht(DHTPIN, DHTTYPE);
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(LEDBUZZERPIN, OUTPUT);
  digitalWrite(LEDBUZZERPIN, LOW);
  Serial.println("Air Quality Safety Monitor Initialized");
}

void loop() {
  delay(2000);
  float humidity = dht.readHumidity();
  float temperature = dht.readTemperature();
  int gasLevel = analogRead(MQ135PIN);
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Failed to read from DHT22 sensor!");
    return;
  }
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" °C | Humidity: ");
  Serial.print(humidity);
  Serial.print(" % | Gas Level: ");
  Serial.println(gasLevel);
  if (gasLevel > GASTHRESHOLD && humidity > HUMIDITYTHRESHOLD) {
    digitalWrite(LEDBUZZERPIN, HIGH);
    Serial.println("ALERT Status: UNSAFE! (High Gas & High Humidity)");
  } else {
    digitalWrite(LEDBUZZERPIN, LOW);
    Serial.println("ALERT Status: Safe");
  }
}
