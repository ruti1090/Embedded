#include <Arduino.h>
#include <ESP32Servo.h>   // שינוי כאן! במקום Servo.h
#include <DHT.h>
#include <DFRobotDFPlayerMini.h>
#include "HX711.h"

// --- הגדרות פינים מהקוד שלך ---

// חיישן טמפרטורה ולחות (DHT11)
#define DHTPIN 23    // GPIO23
#define DHTTYPE DHT11 // או DHT22, לפי מה שיש לך
DHT dht(DHTPIN, DHTTYPE);

// מנועי סרוו
Servo servo1;
Servo servo2;
Servo servo3;
const int SERVO_PIN_1 = 12; // GPIO12
const int SERVO_PIN_2 = 13; // GPIO13
const int SERVO_PIN_3 = 14; // GPIO14

// רמקול MP3 (DFPlayer Mini)
HardwareSerial mySerial(2); // משתמשים ב-UART2 (GPIO16 RX, GPIO17 TX)
DFRobotDFPlayerMini myDFPlayer;

// חיישני מרחק (HC-SR04)
const int TRIG_PIN_1 = 27; // GPIO27
const int ECHO_PIN_1 = 26; // GPIO26

const int TRIG_PIN_2 = 25; // GPIO25
const int ECHO_PIN_2 = 33; // GPIO33

const int TRIG_PIN_3 = 32; // GPIO32
const int ECHO_PIN_3 = 35; // GPIO35

// חיישן משקל (Load Cell + HX711)
const int LOADCELL_DOUT_PIN = 5;  // GPIO5
const int LOADCELL_SCK_PIN = 18;  // GPIO18
HX711 scale;

// --- סוף הגדרות פינים ---

void setup() {
  Serial.begin(115200); // תקשורת טורית למחשב לצורך דיבוג
  Serial.println("--- אתחול מערכת ESP32 ---");

  // --- הגדרות סרוו עבור ESP32Servo ---
  // ESP32Servo מאפשרת להגדיר את מספר המקסימלי של סרווים וטיימרים
  // לא חובה לקרוא לזה אם לא נתקלים בבעיות עם סרוו
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  // הגדרת תדר PWM ל-50Hz (סטנדרטי לסרוו)
  servo1.setPeriodHertz(50);
  servo2.setPeriodHertz(50);
  servo3.setPeriodHertz(50);
  // הגדרת טווח מיקרו-שניות לסרוו (סטנדרטי: 500-2500)
  servo1.attach(SERVO_PIN_1, 500, 2500); // פין, Min Pulse Width, Max Pulse Width
  servo2.attach(SERVO_PIN_2, 500, 2500);
  servo3.attach(SERVO_PIN_3, 500, 2500);

  // הזזת סרוו למצב התחלתי (0 מעלות)
  servo1.write(0);
  servo2.write(0);
  servo3.write(0);
  Serial.println("Servo Motors Initialized to 0 degrees.");
  // --- סיום הגדרות סרוו ---


  // אתחול DHT
  dht.begin();
  Serial.println("DHT Sensor Initialized.");


  // אתחול DFPlayer Mini
  mySerial.begin(9600, SERIAL_8N1, 16, 17); // GPIO16=RX2, GPIO17=TX2
  Serial.println("Initializing DFPlayer Mini...");
  if (!myDFPlayer.begin(mySerial, true, false)) { // Use mySerial, wait for feedback, don't reset
    Serial.println("DFPlayer Mini error. Check wiring.");
  } else {
    Serial.println("DFPlayer Mini initialized successfully.");
    myDFPlayer.volume(25); // Set volume (0-30)
    myDFPlayer.setTimeOut(500); // Set timeout
  }

  // אתחול חיישני מרחק
  pinMode(TRIG_PIN_1, OUTPUT);
  pinMode(ECHO_PIN_1, INPUT);
  pinMode(TRIG_PIN_2, OUTPUT);
  pinMode(ECHO_PIN_2, INPUT);
  pinMode(TRIG_PIN_3, OUTPUT);
  pinMode(ECHO_PIN_3, INPUT);
  Serial.println("Distance Sensors Initialized.");

  // אתחול HX711 (חיישן משקל)
  Serial.print("Initializing HX711 on DOUT:");
  Serial.print(LOADCELL_DOUT_PIN);
  Serial.print(" SCK:");
  Serial.println(LOADCELL_SCK_PIN);
  scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
  if (scale.is_ready()) {
    Serial.println("HX711 is ready.");
    // כאן אפשר להגדיר Calibration factor או Offset
    // scale.set_scale(CALIBRATION_FACTOR); // החלף בערך הכיול שלך
    // scale.tare(); // איפוס לקריאת 0 (ריק)
  } else {
    Serial.println("HX711 not found or not ready.");
  }

  Serial.println("--- סיום אתחול SETUP ---");
}

void loop() {
  // כאן הקוד הראשי שלך ירוץ ויבצע פעולות באופן שוטף
  // לדוגמה, קריאה רגילה מחיישנים והפעלת רכיבים

  // נניח שאת רוצה "לאפס" את המערכת כל 30 שניות:
  delay(30000);
  resetAllSensorsAndActuators();
}

// --- פונקציית האיפוס/אתחול מחדש ---
void resetAllSensorsAndActuators() {
  Serial.println("\n--- מפעיל 'איפוס' לכל החיישנים והרכיבים ---");

  // 1. "איפוס" חיישן טמפרטורה ולחות (קריאה חדשה)
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("שגיאה בקריאה מחיישן DHT!");
    // אם יש בעיות חמורות, ניתן לנסות לאתחל את ה-DHT מחדש:
    // dht.begin();
  } else {
    Serial.print("DHT טמפרטורה: ");
    Serial.print(t);
    Serial.print(" *C, לחות: ");
    Serial.print(h);
    Serial.println(" %");
  }

  // 2. "איפוס" מנועי סרוו (החזרה למצב התחלתי)
  servo1.write(0); // הזזה ל-0 מעלות
  servo2.write(0);
  servo3.write(0);
  Serial.println("מנועי סרוו חזרו ל-0 מעלות.");

  // 3. "איפוס" רמקול MP3 (עצירה, איפוס ווליום)
  myDFPlayer.stop(); // עוצר כל ניגון
  myDFPlayer.volume(25); // מחזיר לווליום התחלתי
  // myDFPlayer.reset(); // זו פקודה חזקה יותר לאיפוס המודול, השתמש רק אם נתקע
  Serial.println("רמקול MP3 אותחל (עוצר ניגון ומאפס ווליום).");

  // 4. "איפוס" חיישני מרחק (קריאה חדשה)
  // פונקציית עזר לקריאת מרחק
  auto readDistance = [](int trigPin, int echoPin) {
    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    long duration = pulseIn(echoPin, HIGH, 10000); // 10ms timeout
    if (duration == 0) return -1.0; // Timeout
    return duration * 0.034 / 2.0;
  };

  Serial.print("חיישן מרחק 1: ");
  float dist1 = readDistance(TRIG_PIN_1, ECHO_PIN_1);
  if (dist1 != -1.0) { Serial.print(dist1); Serial.println(" ס\"מ"); } else { Serial.println("שגיאת קריאה."); }

  Serial.print("חיישן מרחק 2: ");
  float dist2 = readDistance(TRIG_PIN_2, ECHO_PIN_2);
  if (dist2 != -1.0) { Serial.print(dist2); Serial.println(" ס\"מ"); } else { Serial.println("שגיאת קריאה."); }

  Serial.print("חיישן מרחק 3: ");
  float dist3 = readDistance(TRIG_PIN_3, ECHO_PIN_3);
  if (dist3 != -1.0) { Serial.print(dist3); Serial.println(" ס\"מ"); } else { Serial.println("שגיאת קריאה."); }

  // 5. "איפוס" חיישן משקל (קריאת ערך נוכחי ואיפוס אם לא זמין)
  Serial.print("HX711 קריאה: ");
  if (scale.is_ready()) {
    Serial.println(scale.get_value()); // או scale.get_units() אם מכויל
  } else {
    Serial.println("HX711 לא זמין! מנסה לאתחל מחדש...");
    scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN); // מנסה לאתחל מחדש
    if (scale.is_ready()) {
      Serial.println("HX711 אותחל מחדש.");
      // scale.tare(); // בצע איפוס לקריאת 0 אם צריך
    } else {
      Serial.println("HX711 עדיין לא זמין.");
    }
  }

  Serial.println("--- סיום 'איפוס' לכל החיישנים והרכיבים ---");
}