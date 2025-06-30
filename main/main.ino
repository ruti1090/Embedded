#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>  
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include <HardwareSerial.h>      // נשתמש ב-HardwareSerial עבור ESP32
#include <DFRobotDFPlayerMini.h> // ספריית ה-DFPlayer Mini
#include <ESP32Servo.h>
#include "DHT.h"

//חיבור ל WIFI :
const char* ssid = "RutiTikochinski"; // שם הרשת האלחוטית (SSID)
const char* password = "325672657";    // סיסמת הרשת האלחוטית
WebServer server(80); // הפעלת שרת על פורט 80
// מבנה נתונים עבור תרופה
struct Drug {
    String name;
    String time;
    int bottle;
};

// רשימה גלובלית לשמירת התרופות
Drug drugs[3];
int numDrugs = 0; // מונה התרופות הנוכחי ברשימה

// פונקציית עזר להדפסת רשימת התרופות הנוכחית
void printDrugs()
{
  Serial.println("Current Drug Schedule:");
  if (numDrugs == 0) {
    Serial.println("No drugs scheduled.");
    return;
  }
  for (int i = 0; i < numDrugs; i++) {
    Serial.printf("Drug %d: Name=%s, Time=%s, Bottle=%d\n", i + 1, drugs[i].name.c_str(), drugs[i].time.c_str(), drugs[i].bottle);
  }
}

// פונקציה שמטפלת בבקשות POST ל- /update-drugs
void handleUpdateDrugs() 
{
  Serial.println("Received a POST request for /update-drugs");

  // וודא ש Content-Type הוא application/json
  if (server.hasHeader("Content-Type") && server.header("Content-Type").indexOf("application/json") == -1) {
    Serial.printf("Error: Content-Type is not application/json. Received: %s\n", server.header("Content-Type").c_str());
    server.send(400, "text/plain", "Bad Request: Content-Type must be application/json");
    return;
  }

  // בדוק אם גוף הבקשה ריק
  if (!server.hasArg("plain")) {
    Serial.println("Error: Request body is empty.");
    server.send(400, "text/plain", "Bad Request: Request body is empty.");
    return;
  }

  String requestBody = server.arg("plain");
  Serial.print("Received request body: ");
  Serial.println(requestBody);

  // הקצאת זיכרון עבור המסמך JSON
  StaticJsonDocument<1024> doc; 

  // ניתוח ה-JSON
  DeserializationError error = deserializeJson(doc, requestBody);

  if (error) 
  {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    server.send(400, "text/plain", "Failed to parse JSON: " + String(error.f_str()));
    return;
  }

  // ניקוי רשימת התרופות הקיימת
  numDrugs = 0;

  // קריאת המערך "drugs" מה-JSON
  JsonArray drugsArray = doc["drugs"];

  if (drugsArray.isNull()) 
  {
    Serial.println("Error: 'drugs' array not found in JSON.");
    server.send(400, "text/plain", "Bad Request: 'drugs' array missing.");
    return;
  }
  // מעבר על התרופות במערך ועדכון הרשימה הגלובלית
  for ( JsonVariant v : drugsArray) 
  {
    if(numDrugs<=3)
    {
      drugs[numDrugs].name = v["name"].as<String>();
      drugs[numDrugs].time = v["time"].as<String>();
      drugs[numDrugs].bottle = v["bottle"].as<int>();
      numDrugs++;
    } 
    else 
    {
      Serial.println("Warning: Max drugs capacity reached. Some drugs might not be stored.");
    }
  }

  Serial.println("Drugs updated successfully!");
  printDrugs(); // הדפס את הרשימה המעודכנת
  server.send(200, "text/plain", "Drugs schedule updated successfully!");

  //חיבור לשרת השעה הנוכחית :

  // הגדרות NTP server
const long gmtOffset_sec = 3 * 3600; // GMT+3 (ישראל שעון קיץ)
const int daylightOffset_sec = 0; 
const char* ntpServer = "pool.ntp.org";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

// פונקציה חדשה לעדכון וטיפול בזמן NTP
void updateNTPTime()
 {
  static unsigned long lastNTPUpdate = 0;
  const long ntpUpdateInterval = 15 * 60 * 1000;
  // וודא חיבור WiFi לפני ניסיון עדכון NTP
  if (WiFi.status() != WL_CONNECTED)
 {
    Serial.println("WiFi לא מחובר, לא ניתן לעדכן NTP כעת. סטטוס WiFi: " + String(WiFi.status()));
    return;
 }

  // עדכן NTP רק אם עבר מספיק זמן או בפעם הראשונה
 if (millis() - lastNTPUpdate >= ntpUpdateInterval) 
{
 Serial.println("מתחיל עדכון זמן NTP...");
  if (timeClient.update()) {
    Serial.print("הזמן סונכרן בהצלחה: ");
    Serial.println(timeClient.getFormattedTime());
    Serial.print("Epoch Time (UTC): ");
    Serial.println(timeClient.getEpochTime());
    Serial.print("יום בשבוע: ");
    Serial.println(timeClient.getDay());
  } else 
  {
   Serial.println("נכשל עדכון זמן NTP.");
  }
  lastNTPUpdate = millis();
  }
}
}

//טמפרטורה ולחות :
#define DHTPIN 26
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
// הגדרת ספים מומלצים לטמפרטורה ולחות לתרופות
const float MAX_TEMP_ALLOWED = 25.0;
const float MIN_TEMP_ALLOWED = 15.0;
const float MAX_HUMIDITY_ALLOWED = 60.0;
const float MIN_HUMIDITY_ALLOWED = 30.0;

//חיישן מרחק כללי :
const int IRSensorPin = 23; 
 
 //מרחק עבור 3 התאים :
 // הגדרת פיני GPIO לחיישני מרחק IR עבור הבקבוקים
  const int IRSensorPin1 = 33;
  const int IRSensorPin2 = 32;
  const int IRSensorPin3 = 34;

  //דופק
  #define ECG_PIN 27  // שינוי מ-25 ל-34 (פין ADC)
  const int ECG_THRESHOLD = 700;
  unsigned long lastECGPeak = 0;
  float bpm = 0;
  float normalizedBPM = 77.0;
 
  //חום גוף :
  #define TMP102_ADDR 0x4C 

  //רמקול :
  #define DFPLAYER_TX_PIN 2
  #define DFPLAYER_RX_PIN 4
  // יצירת אובייקט HardwareSerial. נשתמש ב-UART2
  HardwareSerial myDFPlayerSerial(2);
  
  DFRobotDFPlayerMini myDFPlayer;
  bool dfPlayerReady = false; // דגל כדי לדעת אם ה-DFPlayer אותחל בהצלחה
  
  // מפה בין הודעות קוליות למספרי קבצים ב-DFPlayer Mini
  const int AUDIO_GOOD_NIGHT = 1;      // 0001.mp3
  const int AUDIO_GOOD_EVENING = 2;    // 0002.mp3
  const int AUDIO_GOOD_MORNING = 3;    // 0003.mp3
  const int AUDIO_GOOD_AFTERNOON = 4;  // 0004.mp3
  const int  AUDIO_PAY_ATTENTION = 5;   // 0005.mp3 (לשים לב/תרופה ממתינה)
  
  //מנועי סרוו
  // הגדרת פיני ה-GPIO שאליהם מחוברים מנועי הסרוו
  #define SERVO1_PIN 18 
  #define SERVO2_PIN 19 
  #define SERVO3_PIN 13 

// יצירת אובייקטים לכל מנוע סרוו
// לכל מנוע סרוו שתחבר, צור אובייקט Servo חדש
Servo myServo1;
Servo myServo2;
Servo myServo3;
  
  // הגדרת מקסימום התרופות שניתן לשמור
#define MAX_DRUGS 3

// מבנה (struct) לייצוג תרופה בודדת
struct Drug {
  String name;
  String time; // "HH:MM"
  int bottle;
  bool dispensed;
};

// מערך לאחסון התרופות
Drug drugSchedule[MAX_DRUGS];
int numDrugs = 0;

//אפשרות להוספת כותרות לשליחת הנתונים כדי שלא ייחסמו מטעמי אבטחה
 void addCORSHeaders() 
 {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

// פונקציה לבדיקת חיישן מרחק עבור בקבוק ספציפי
bool isDrugDetected(int bottleNumber)
 {
  int sensorPin;
  int numReads = 5;
  int requiredDetections = 3;
  const int readDelay = 10;

  switch (bottleNumber) {
    case 1: sensorPin = IRSensorPin1; break;
    case 2: sensorPin = IRSensorPin2; break;
    case 3: sensorPin = IRSensorPin3; break;
    default:
      Serial.println("שגיאה: מספר בקבוק לא חוקי לבדיקת חיישן!");
      return false;
  }

  int readings = 0;
  for (int i = 0; i < numReads; i++) {
    if (digitalRead(sensorPin) == LOW) 
    { // LOW = עצם זוהה
      readings++;
    }
    delay(readDelay);
  }
  Serial.print("חיישן "); Serial.print(bottleNumber);
  Serial.print(" זיהה "); Serial.print(readings); Serial.print(" מתוך "); Serial.print(numReads); Serial.print(" פעמים. נדרש: "); Serial.print(requiredDetections); Serial.println(".");
  return readings >= requiredDetections;
}



void setup() 
{
  Serial.begin(115200);  
  Serial.println("ESP32 Started!");  
  //WIFI :
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi....");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.println("WiFi connected.");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // הגדרת נתיב ה-API
  server.on("/update-drugs", HTTP_POST, handleUpdateDrugs);

  // טיפול בבקשות לא ידועות (404 Not Found)
  server.onNotFound([]() {
    Serial.print("Received unknown request: ");
    Serial.println(server.uri());
    server.send(404, "text/plain", "Not Found");
  });

  server.begin();
  Serial.println("HTTP server started");

  //שעה נוכחית :
  timeClient.begin();
  Serial.println("מבצע סנכרון NTP ראשוני ב-setup...");
  bool ntpSynced = false;
  for (int i = 0; i < 5; i++) {
    if (timeClient.update()) {
      Serial.print("הזמן סונכרן בהצלחה ב-setup: ");
      Serial.println(timeClient.getFormattedTime());
      ntpSynced = true;
      break;
    } else {
      Serial.print("ניסיון סנכרון NTP #"); Serial.print(i + 1); Serial.println(" נכשל. ממתין...");
      delay(1000);
    }
  }

  //טמפרטורה ולחות :
  // Serial.begin(9600); // התחלת תקשורת טורית
  dht.begin(); // התחלת החיישן

  //מרחק כללי
  pinMode(IRSensorPin, INPUT); // הגדרת פין החיישן כפין קלט
  Serial.println("IR Proximity Sensor Test (Digital Output)");
  Serial.println("=========================================");
  Serial.println("Approaching an object to the sensor...");

  //דופק
  pinMode(ECG_PIN, INPUT);
  

  //חום גוף :
   Wire.begin();

   //רמקול :
   Serial.println("--- מתחיל בדיקת DFPlayer Mini ---");
   myDFPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
   Serial.println("מנסה לאתחל DFPlayer Mini...");
   // ניסיון לאתחל את ה-DFPlayer Mini.
  // אם .begin() מחזיר false, משהו לא עובד.
  if (!myDFPlayer.begin(myDFPlayerSerial)) {
    Serial.println("שגיאה: DFPlayer Mini לא אותחל!");
    Serial.println("בדוק את הדברים הבאים:");
    Serial.println("1. חיווט: ודא ש-TX של ESP32 (GPIO2) מחובר ל-RX של DFPlayer, ו-RX של ESP32 (GPIO4) מחובר ל-TX של DFPlayer.");
    Serial.println("2. מתח: ודא ש-DFPlayer מקבל 5V יציבים ושיש חיבור אדמה משותף עם ה-ESP32.");
    Serial.println("3. כרטיס SD: ודא שכרטיס MicroSD בפורמט FAT32, מוכנס היטב, ויש עליו תיקיית 'mp3'.");
    Serial.println("4. קבצי אודיו: ודא שיש קבצי MP3 בתיקיית 'mp3' בשמות כמו 0001.mp3, 0002.mp3, וכו'.");
    // עצירה כאן אם האתחול נכשל, כדי לא לנסות לנגן.
    while(true); 
  }
  Serial.println("DFPlayer Mini אותחל בהצלחה!");
  // הגדרת עוצמת השמע. ערך בין 0 ל-30.
  myDFPlayer.volume(30); // קובע את העוצמה ל-30 (מקסימום).
  Serial.print("עוצמת שמע הוגדרה ל: ");
  // --- בדיקת נגינה: נגן קובץ 0001.mp3 ---
  Serial.println("מבצע בדיקת נגינה של קובץ 0001.mp3...");
  myDFPlayer.play(1); // נגן את קובץ מספר 1
  delay(3000); // המתן 3 שניות כדי לתת לקובץ להתנגן
  // נגן קובץ 2
  Serial.println("מבצע בדיקת נגינה של קובץ 0002.mp3...");
  myDFPlayer.play(2); // נגן את קובץ מספר 2
  delay(3000); // המתן 3 שניות כדי לתת לקובץ להתנגן
  Serial.println("--- בדיקת DFPlayer Mini הסתיימה ב-setup ---");

  //סרוו
  Serial.println("אתחול מנועי סרוו...");
  myServo1.attach(18);
  myServo2.attach(19);
  myServo3.attach(13);

  myServo1.write(0);
  myServo2.write(0);
  myServo3.write(0);


}

void loop()
 {
  //WIFI:
  server.handleClient(); // טיפול בבקשות HTTP נכנסות
  //שעה נוכחית
  updateNTPTime();
  //טמפרטורה ולחות :
  float humidity = dht.readHumidity(); // קריאת לחות
  float temperature = dht.readTemperature(); // קריאת טמפרטורה 
  if (isnan (humidity) || isnan (temperature)) 
  {
    Serial.println("נכשל בקריאה מחיישן DHT!");
  }
  float heatIndex = dht.computeHeatIndex (temperature, humidity, false);
  Serial.print("לחות: ");
  Serial.println(humidity);
  Serial.print("טמפרטורה באחוזים: ");
  Serial.println(temperature);
  }

  //מרחק כללי :
  int sensorValue = digitalRead(IRSensorPin); 
  if (sensorValue == LOW) { // אם האות LOW (אובייקט זוהה)
    Serial.println("Object detected!");
  }
   else { // אם האות HIGH (אין אובייקט)
    Serial.println("No object detected.");
  }
 //דופק :
 int ecgSum = 0;
  for (int i = 0; i < 5; ++i) {
    ecgSum += analogRead(ECG_PIN);
    delay(2);
  }

  int ecgVal = ecgSum / 5;
  unsigned long now = millis();
  
  if (ecgVal > ECG_THRESHOLD && (now - lastECGPeak) > 500) {
    Serial.println("ecgVal");
    Serial.println(ecgVal);
    Serial.println(lastECGPeak);

    bpm = 60000.0 / (now - lastECGPeak);
    lastECGPeak = now;
    normalizedBPM = constrain(bpm, 50.0, 120.0);
    
    Serial.print("BPM: ");
    Serial.println(normalizedBPM);
  }
  
  //חום גוף :
  Wire.beginTransmission(TMP102_ADDR);
  Wire.write(0x00);  // רגיסטר טמפרטורה
  Wire.endTransmission();

  Wire.requestFrom(TMP102_ADDR, 2);
  if (Wire.available() == 2)
  {
    int16_t raw = (Wire.read() << 4) | (Wire.read() >> 4);
    float tempC = raw * 0.0625;
    Serial.print("טמפרטורה: ");
    Serial.print(tempC);
    Serial.println(" °C");
  } 
  else
  {
    Serial.println("לא התקבלו נתונים מהחיישן");
  }

  //רמקול :

}