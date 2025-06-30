#include "HX711.h"

// הגדרת פינים - ודאי שאלה הפינים הנכונים שחיברת ל-ESP32
#define DT_PIN 21
#define SCK_PIN 22

HX711 scale;

void setup() {
  Serial.begin(115200);
  delay(1000); 
  scale.begin(DT_PIN, SCK_PIN); // אתחול HX711

  Serial.println("=========================================");
  Serial.println("שלב 2: קריאת נתונים גולמיים לכיול");
  Serial.println("=========================================");
  Serial.println("וודא שאין שום משקל על החיישן כעת.");
  Serial.println("הקריאות הגולמיות יוצגו להלן:");
  Serial.println("-----------------------------------------");
  delay(2000); 
}

void loop() {
  if (scale.is_ready()) {
    long reading = scale.read(); // קורא את הערך הגולמי
    Serial.print("קריאה גולמית: ");
    Serial.println(reading);
  } else {
    Serial.println(">>> חיישן HX711 לא מוכן! בדוק חיבורים. <<<");
  }
  delay(100); // קורא כל 100 מילישניות
}