// הגדרת הפינים שאליהם מחוברים פיני ה-DO (פלט דיגיטלי) של חיישני ה-IR
const int IRSensorPin1 = 33; // חיישן IR 1 מחובר ל-GPIO 25
const int IRSensorPin2 = 32; // חיישן IR 2 מחובר ל-GPIO 33
const int IRSensorPin3 = 34; // חיישן IR 3 מחובר ל-GPIO 32

void setup() {
  Serial.begin(115200); // אתחול תקשורת סריאלית
  pinMode(IRSensorPin1, INPUT); // הגדרת פין 25 כפין קלט
  pinMode(IRSensorPin2, INPUT); // הגדרת פין 33 כפין קלט
  pinMode(IRSensorPin3, INPUT); // הגדרת פין 32 כפין קלט

  Serial.println("IR Proximity Sensors Test (Digital Output)");
  Serial.println("=========================================");
  Serial.println("Approaching objects to the sensors...");
}

void loop() {
  int sensorValue1 = digitalRead(IRSensorPin1); // קריאת מצב חיישן 1
  int sensorValue2 = digitalRead(IRSensorPin2); // קריאת מצב חיישן 2
  int sensorValue3 = digitalRead(IRSensorPin3); // קריאת מצב חיישן 3

  Serial.print("חיישן 1 (GPIO25): ");
  if (sensorValue1 == LOW) { // אם האות LOW (אובייקט זוהה)
    Serial.println("Object detected!");
  } else { // אם האות HIGH (אין אובייקט)
    Serial.println("No object detected.");
  }

  Serial.print("חיישן 2 (GPIO33): ");
  if (sensorValue2 == LOW) { // אם האות LOW (אובייקט זוהה)
    Serial.println("Object detected!");
  } else { // אם האות HIGH (אין אובייקט)
    Serial.println("No object detected.");
  }

  Serial.print("חיישן 3 (GPIO32): ");
  if (sensorValue3 == LOW) { // אם האות LOW (אובייקט זוהה)
    Serial.println("Object detected!");
  } else { // אם האות HIGH (אין אובייקט)
    Serial.println("No object detected.");
  }

  Serial.println("---"); // מפריד בין קריאות
  delay(200); // השהייה קצרה בין קריאות
}