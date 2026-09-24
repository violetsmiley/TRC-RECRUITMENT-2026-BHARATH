void setup() {
  Serial.begin(9600);
}

void loop() {
  Serial.println("RED");
  delay(10000); 
  Serial.println("GREEN");
  delay(7000);
  Serial.println("YELLOW");
  delay(3000); 
}