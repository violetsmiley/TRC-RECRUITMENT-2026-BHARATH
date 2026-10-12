void setup() {
  Serial.begin(9600);
}

void loop() {
  float Temperature = random(15, 51);
  Serial.print("current Temperature: ");
  Serial.print(Temperature);
  Serial.println(" C");
  if (Temperature > 40.0) {
    Serial.println("Fan ON");
  } 
  else if (Temperature < 25.0) {
    Serial.println("Fan OFF");
  } 
  else {
    Serial.println("Fan in Standby");
  }
  delay(2000);
}