#include <Servo.h>

Servo myservo;  // יוצר אובייקט סרוו לשליטה במנוע

int servoPin = 4; // פין ה-GPIO של ESP32 שמחובר לחוט האות של הסרוו
int angle = 0;    // משתנה לשמירת הזווית הנוכחית של הסרוו

void setup() {
  myservo.attach(servoPin); // מקשר את אובייקט הסרוו לפין שבחרנו
  Serial.begin(115200);     // אתחול תקשורת טורית לדיבאג
  Serial.println("התחלת שליטה בסרוו");
}

void loop() {
  // סריקה מ-0 ל-180 מעלות
  Serial.println("סריקה קדימה...");
  for (angle = 0; angle <= 180; angle += 1) {
    Serial.print("זווית: ");
    Serial.println(angle);
    myservo.write(angle); // כותב את הזווית לסרוו
    delay(15);           // זמן המתנה קצר לתנועה חלקה
  }

  // סריקה מ-180 ל-0 מעלות
  Serial.println("סריקה אחורה...");
  for (angle = 180; angle >= 0; angle -= 1) {
    Serial.print("זווית: ");
    Serial.println(angle);
    myservo.write(angle); // כותב את הזווית לסרוו
    delay(15);           // זמן המתנה קצר לתנועה חלקה
  }

  delay(1000); // המתנה של שנייה לפני הסריקה הבאה
}