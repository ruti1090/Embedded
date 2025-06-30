const int touchPin1 = 33; // חיישן מגע 1 (מחובר ל-D33)
const int touchPin2 = 32; // חיישן מגע 2 (מחובר ל-D32)
const int touchPin3 = 35; // חיישן מגע 3 (מחובר ל-D35)

void setup() {
  Serial.begin(115200); // אתחול התקשורת הטורית
  pinMode(touchPin1, INPUT); // הגדרת פין D33 כקלט
  pinMode(touchPin2, INPUT); // הגדרת פין D32 כקלט
  pinMode(touchPin3, INPUT); // הגדרת פין D35 כקלט
}

void loop() {
  int touchValue1 = digitalRead(touchPin1); // קריאת מצב חיישן 1
  int touchValue2 = digitalRead(touchPin2); // קריאת מצב חיישן 2
  int touchValue3 = digitalRead(touchPin3); // קריאת מצב חיישן 3

  // הדפסת מצב חיישן 1
  Serial.print("חיישן D33: ");
  if (touchValue1 == HIGH) {
    Serial.println("נגיעה זוהתה!");
  } else {
    Serial.println("אין נגיעה");
  }

  // הדפסת מצב חיישן 2
  Serial.print("חיישן D32: ");
  if (touchValue2 == HIGH) {
    Serial.println("נגיעה זוהתה!");
  } else {
    Serial.println("אין נגיעה");
  }

  // הדפסת מצב חיישן 3
  Serial.print("חיישן D35: ");
  if (touchValue3 == HIGH) {
    Serial.println("נגיעה זוהתה!");

  } else {
    Serial.println("אין נגיעה");

  }

  Serial.println("---"); // מפריד בין קריאות
  delay(200); // השהיה קצרה בין קריאות
}