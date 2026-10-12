const int LDRPIN = A0;
const int LEDPIN = 13;
const int DARKLIMIT = 500; 

void setup() {
  pinMode(LEDPIN, OUTPUT);
  Serial.begin(9600);
}
void loop() {
  int ldrValue = analogRead(LDRPIN);
  Serial.print("LDR Light Value:");
  Serial.println(ldrValue);
  if (ldrValue < DARKLIMIT) {
    digitalWrite(LEDPIN, HIGH);  
  } else {
    digitalWrite(LEDPIN, LOW);  
  }
    delay(200);
}