const int TRIG_PIN = 5;
const int ECHO_PIN = 18;
const int LDR_PIN = 34;
const int LED_PIN = 4;
unsigned long previousMillis = 0;
const long blinkInterval = 150;
bool ledState = false;
void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  Serial.println("Smart Parking Assistant Initialized.");
}
void loop() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  long duration = pulseIn(ECHO_PIN, HIGH);
  float distance = duration * 0.0343 / 2;
  int ldrValue = analogRead(LDR_PIN); 
  int brightness = map(ldrValue, 0, 4095, 255, 10); 
  String ledStatus = "";
  if (distance < 10.0) {
    analogWrite(LED_PIN, brightness);
    ledStatus = "Solid ON (Danger!)";
  } 
  else if (distance < 20.0) {
    unsigned long currentMillis = millis();
    if (currentMillis - previousMillis >= blinkInterval) {
      previousMillis = currentMillis;
      ledState = !ledState;
    }
    if (ledState) {
      analogWrite(LED_PIN, brightness);
    } else {
      analogWrite(LED_PIN, 0);
    }
    ledStatus = "Blinking Fast (Warning)";
  } 
  else {
    analogWrite(LED_PIN, 0);
    ledStatus = "OFF (Safe)";
  }
  Serial.print("Distance: ");
  Serial.print(distance);
  Serial.print(" cm | LDR Light Value: ");
  Serial.print(ldrValue);
  Serial.print(" | LED Status: ");
  Serial.println(ledStatus);
  delay(60); 
}