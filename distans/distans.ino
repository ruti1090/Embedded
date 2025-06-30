// הגדרת הפין שאליו מחובר פין ה-DO (פלט דיגיטלי) של חיישן ה-IR
const int IRSensorPin = 23; // שימוש ב-GPIO 23 כדוגמה. ניתן לשנות לפין דיגיטלי אחר ב-ESP32.

void setup() {
  Serial.begin(115200); // אתחול תקשורת סריאלית
  pinMode(IRSensorPin, INPUT); // הגדרת פין החיישן כפין קלט
  Serial.println("IR Proximity Sensor Test (Digital Output)");
  Serial.println("=========================================");
  Serial.println("Approaching an object to the sensor...");
}

void loop() {
  int sensorValue = digitalRead(IRSensorPin); 

  if (sensorValue == LOW) { // אם האות LOW (אובייקט זוהה)
    Serial.println("Object detected!");
  } else { // אם האות HIGH (אין אובייקט)
    Serial.println("No object detected.");
  }

  delay(200); // השהייה קצרה בין קריאות (200 מילישניות)
}