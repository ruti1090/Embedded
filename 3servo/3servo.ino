#include <ESP32Servo.h> // ספריית סרוו עבור ESP32

// הגדרת פיני ה-GPIO שאליהם מחוברים מנועי הסרוו
// ודא שפינים אלו תואמים את החיבורים הפיזיים שלך!
#define SERVO1_PIN 18 // סרוו 1 מחובר ל-GPIO18
#define SERVO2_PIN 19 // סרוו 2 מחובר ל-GPIO19
#define SERVO3_PIN 13 // סרוו 3 מחובר ל-GPIO21

// יצירת אובייקטים לכל מנוע סרוו
// לכל מנוע סרוו שתחבר, צור אובייקט Servo חדש
Servo myServo1;
Servo myServo2;
Servo myServo3;

void setup() {
  Serial.begin(115200); // אתחול תקשורת טורית למחשב לצורך הדפסת הודעות

  Serial.println("אתחול מנועי סרוו...");
  myServo1.attach(18);
  myServo2.attach(19);
  myServo3.attach(13);

  myServo1.write(0);
  myServo2.write(0);
  myServo3.write(0);


}

void loop() {
  // לולאה ראשונה: מזיזה את הסרוואים מ-0 ל-180 מעלות (או חלק מזה)
  for (int pos = 0; pos <= 150; pos += 1) { // הלולאה סופרת מ-0 עד 180 בצעדים של 1
    myServo1.write(pos);                     // סרוו 1 נע מ-0 ל-180 מעלות
    myServo2.write(pos);               // סרוו 2 נע מ-180 ל-0 מעלות (בכיוון הפוך)
    myServo3.write(pos);                 // סרוו 3 נע מ-0 ל-90 מעלות (חצי טווח, איטי יותר)
    delay(15); // השהיה קצרה (15 אלפיות השנייה) לייצוב תנועת הסרוו
  }

  // לולאה שנייה: מזיזה את הסרוואים מ-180 ל-0 מעלות (או חלק מזה)
  for (int pos = 150; pos >= 0; pos -= 1) { // הלולאה סופרת מ-180 עד 0 בצעדים של 1
    myServo1.write(pos);
    myServo2.write( pos);
    myServo3.write(pos);
    delay(15);
  }
}