#include <WiFi.h> //ויפי
#include <WebServer.h> //אפליקציה
#include <ArduinoJson.h> //שימוש בג'יסון
#include <NTPClient.h> //שרת השעות
#include <WiFiUdp.h> //העברת נתונים באינטרנט
#include <Adafruit_Sensor.h>  //לשימוש של חיישנים נותנת מבנה אחיד 
#include <Wire.h>  //תקשורת I2C
#include <HardwareSerial.h>     //בשביל הסריאל
#include <DFRobotDFPlayerMini.h> //לרמקול
#include <ESP32Servo.h>          //מנועי סרוו
#include "DHT.h"     //טמפרטורה ולחות

// מבנה לתרופה
const int MAX_DRUGS = 3; 
struct Drug {
  String name;
  String time; // "HH:MM"
  int bottle;
  bool dispensed;
};

//מערך תרופות
Drug drugSchedule[MAX_DRUGS];
int numDrugs = 0;

// DHT11
#define DHTPIN 26
#define DHTTYPE DHT11
//סוג הפין שחיברתי
DHT dht(DHTPIN, DHTTYPE); 

//הגדרה של הטמפרטורה והלחות הנכונה עבור התרופות
const float MAX_TEMP_ALLOWED = 25.0;
const float MIN_TEMP_ALLOWED = 15.0;
const float MAX_HUMIDITY_ALLOWED = 60.0;
const float MIN_HUMIDITY_ALLOWED = 30.0;

//חיבור לויפי
const char* ssid = "Kita-2"; 
const char* password = "Xnhbrrfxho";    
WebServer server(80); 

//התחברות לשרת לקבלת שעות
WiFiUDP ntpUDP;
//המרה לשעה מקומית בישראל
const long gmtOffset_sec = 3 * 3600;
const int daylightOffset_sec = 0; 
NTPClient timeClient(ntpUDP, "pool.ntp.org", gmtOffset_sec, daylightOffset_sec);


// חיישן מרחק עליון
const int GENERAL_IR_PROXIMITY_PIN = 23; 

//חיישן מרחק לכל תא
const int IRSensorPin1 = 33;
const int IRSensorPin2 = 32;
const int IRSensorPin3 = 34;

// חיישן דופק
#define ECG_PIN 27
//דופק קלאסי
const int ECG_THRESHOLD = 700;
//בדיקה אחרונה
unsigned long lastECGPeak = 0;
float bpm = 0;
//נרמול הדופק
float normalizedBPM = 77.0;

// חיישן חום גוף 
#define TMP102_ADDR 0x4C

// רמקול
#define DFPLAYER_TX_PIN 2
#define DFPLAYER_RX_PIN 4
HardwareSerial myDFPlayerSerial(2);
DFRobotDFPlayerMini myDFPlayer; 
bool dfPlayerReady = false; 

//הגדרה של הודעה למספר הקובץ בו היא נמצאת
const int AUDIO_GOOD_NIGHT = 1;
const int AUDIO_GOOD_EVENING = 2;
const int AUDIO_GOOD_MORNING = 3;
const int AUDIO_GOOD_AFTERNOON = 4;
const int AUDIO_PAY_ATTENTION = 5;

// מנועי סרוו
#define SERVO1_PIN 18
#define SERVO2_PIN 19
#define SERVO3_PIN 13

Servo servoBottle1; 
Servo servoBottle2;
Servo servoBottle3;
 
const int STEP_DELAY = 15; 
const int FINAL_SETTLE_DELAY = 500; 


void addCORSHeaders();
void updateNTPTime();
void handleUpdateDrugsSimple();
bool isDrugDetected(int bottleNumber);
void checkGeneralProximitySensor();
void readAndCheckDHTSensor();
void dispenseDrug(int bottleNumber);
void handleOptions();


// אפשרות להוספת כותרות לשליחת בקשות
void addCORSHeaders() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
}

//קבלת שעה מהשרת NTP
void updateNTPTime()
{
  static unsigned long lastNTPUpdate = 0;
  const long ntpUpdateInterval = 15 * 60 * 1000; 
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi לא מחובר, לא ניתן לעדכן NTP כעת. סטטוס WiFi: " + String(WiFi.status()));
    return;
  }

  // עדכון אם עבר מספיק זמן
  if (millis() - lastNTPUpdate >= ntpUpdateInterval || lastNTPUpdate == 0) 
  { 
    Serial.println("מתחיל עדכון זמן NTP...");
    if (timeClient.update()) 
    {
      Serial.print("הזמן סונכרן בהצלחה: ");
      Serial.println(timeClient.getFormattedTime());
      Serial.print("Epoch Time (UTC): ");
      Serial.println(timeClient.getEpochTime());
      Serial.print("יום בשבוע: ");
      Serial.println(timeClient.getDay());
    } 
    else
    {
      Serial.println("נכשל עדכון זמן NTP.");
    }
    lastNTPUpdate = millis();
  }
}

// פונקציה לבקשות מהאפליקציה ולהפך
void handleUpdateDrugsSimple() 
{
  addCORSHeaders();
  Serial.println("!!!!!!! קיבלתי בקשת POST ל-/update-drugs - הצלחה !!!!!!!");
  if (server.hasArg("plain")) {
    String requestBody = server.arg("plain");
    Serial.print("אורך גוף הבקשה שהתקבל: ");
    Serial.println(requestBody.length());
    Serial.print("תוכן גוף הבקשה שהתקבל: ");
    Serial.println(requestBody);
    StaticJsonDocument<1024> doc; 
    DeserializationError error = deserializeJson(doc, requestBody);
    if (error)
    {
      Serial.print(F("deserializeJson() נכשל: "));
      Serial.println(error.f_str());
      StaticJsonDocument<100> responseDoc;
      responseDoc["status"] = "error";
      responseDoc["message"] = "Failed to parse JSON: " + String(error.f_str());
      String jsonResponse;
      serializeJson(responseDoc, jsonResponse);
      server.send(400, "application/json", jsonResponse);
      return;
    }
    JsonArray drugsArray = doc["drugs"].as<JsonArray>();
    if (!drugsArray) 
    {
      Serial.println("שגיאה: מערך 'drugs' לא נמצא או אינו מערך.");
      StaticJsonDocument<100> responseDoc;
      responseDoc["status"] = "error";
      responseDoc["message"] = "JSON error: 'drugs' array missing or invalid.";
      String jsonResponse;
      serializeJson(responseDoc, jsonResponse);
      server.send(400, "application/json", jsonResponse);
      return;
    }
    numDrugs = 0;
    Serial.println("\n--- מאחסן לוח זמנים חדש לתרופות ---");
    for (JsonObject drug : drugsArray) 
    {
      if (numDrugs < MAX_DRUGS) 
      {
        drugSchedule[numDrugs].name = drug["name"].as<String>();
        drugSchedule[numDrugs].time = drug["time"].as<String>();
        drugSchedule[numDrugs].bottle = drug["bottle"].as<int>();
        drugSchedule[numDrugs].dispensed = false;

        Serial.print("נשמר: שם: ");
        Serial.print(drugSchedule[numDrugs].name);
        Serial.print(", שעה: ");
        Serial.print(drugSchedule[numDrugs].time);
        Serial.print(", בקבוק: ");
        Serial.println(drugSchedule[numDrugs].bottle);

        numDrugs++;
      }
      else 
      {
        Serial.println("אזהרה: הושג המספר המקסימלי של תרופות, מדלג על תרופות נוספות.");
        break;
      }
    }
    Serial.println("--- סיום אחסון ---");
    StaticJsonDocument<100> responseDoc;
    responseDoc["status"] = "success";
    responseDoc["message"] = "Schedule updated successfully!";
    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    server.send(200, "application/json", jsonResponse);
    Serial.println("נשלחה תגובת JSON: " + jsonResponse);

  } 
  else 
  {
    Serial.println("לא התקבל גוף בבקשת POST.");
    StaticJsonDocument<100> responseDoc;
    responseDoc["status"] = "error";
    responseDoc["message"] = "Bad request: No body received.";
    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    server.send(400, "application/json", jsonResponse);
  }
}

// פונקציה לבדיקת חיישן מרחק עבור בקבוק ספציפי
bool isDrugDetected(int bottleNumber)
{
  int sensorPin;
  int numReads = 5;
  int requiredDetections = 3;
  const int readDelay = 10;
  switch (bottleNumber) 
  {
    case 1: sensorPin = IRSensorPin1; break;
    case 2: sensorPin = IRSensorPin2; break;
    case 3: sensorPin = IRSensorPin3; break;
    default:
      Serial.println("שגיאה: מספר בקבוק לא חוקי לבדיקת חיישן!");
      return false;
  }
  int readings = 0;
  for (int i = 0; i < numReads; i++)
  {
    if (digitalRead(sensorPin) == LOW)
    { 
      readings++;
    }
    delay(readDelay);
  }
  Serial.print("חיישן "); Serial.print(bottleNumber);
  Serial.print(" זיהה "); Serial.print(readings); Serial.print(" מתוך "); Serial.print(numReads); Serial.print(" פעמים. נדרש: "); Serial.print(requiredDetections); Serial.println(".");
  return readings >= requiredDetections;
}

// פונקציה לבדיקת חיישן הקרבה הכללי ותגובה
void checkGeneralProximitySensor()
{
  static int lastSensorValue = HIGH;
  static unsigned long lastInteractionTime = 0;
  const long interactionCooldown = 5000;
  int currentSensorValue = digitalRead(GENERAL_IR_PROXIMITY_PIN);
  if (currentSensorValue == LOW && lastSensorValue == HIGH) {
    if (millis() - lastInteractionTime >= interactionCooldown)
    {
      Serial.println("\n--- אדם זוהה מתקרב למערכת! ---");
      bool foundDrugOrAlert = false; 
      //כאשר אדם זוהה נבדוק מה צריך להודיע לו 
      if (timeClient.getEpochTime() != 0) 
      { 
        String currentTime = timeClient.getFormattedTime().substring(0, 5); // HH:MM
        for (int bottleNum = 1; bottleNum <= 3; bottleNum++)
        {
          bool isDrugPending = false;
          String pendingDrugName = "לא ידועה";
          for (int i = 0; i < numDrugs; i++)
          {
            if (drugSchedule[i].bottle == bottleNum) 
            {
              if (currentTime.compareTo(drugSchedule[i].time) >= 0 && !drugSchedule[i].dispensed) 
              {
                isDrugPending = true;
                pendingDrugName = drugSchedule[i].name;
                break;
              }
            }
          }

          if (isDrugPending) 
          {
            if (dfPlayerReady) 
            {
              myDFPlayer.play(0005);
              Serial.print(">> הושמע: 'שים לב! תרופת ה-"); Serial.print(pendingDrugName);
              Serial.print(" מבקבוק "); Serial.print(bottleNum); Serial.println(" ממתינה שתקח אותה.'");
              delay(2000);
            } else {
              Serial.print(">> שים לב! תרופת ה-"); Serial.print(pendingDrugName);
              Serial.print(" מבקבוק "); Serial.print(bottleNum);
              Serial.println(" ממתינה שתקח אותה. אנא ודא/י את הסטטוס שלה.");
            }
            foundDrugOrAlert = true; // סמן שהושמעה הודעה
            break; // צא מלולאת הבקבוקים אחרי שמצאנו תרופה אחת שממתינה
          }
           else if (isDrugDetected(bottleNum)) 
           { 
            String drugName = "לא ידועה";
            for (int i = 0; i < numDrugs; i++) {
              if (drugSchedule[i].bottle == bottleNum) {
                drugName = drugSchedule[i].name;
                break;
              }
            }
            Serial.print(">> שימו לב, נראה שיש פריט בבקבוק "); Serial.print(bottleNum);
            Serial.print(" (אולי "); Serial.print(drugName); Serial.println(").");
            foundDrugOrAlert = true; 
            break; 
          }
        }
      } 
      else
      { 
        Serial.println("הזמן לא סונכרן עדיין, לא ניתן לבדוק תרופות ממתינות.");
      }
      if (!foundDrugOrAlert) 
      {
        if (dfPlayerReady && timeClient.getEpochTime() != 0) 
        { 
          int currentHour = timeClient.getHours();
          if (currentHour >= 5 && currentHour < 12)
          {
            myDFPlayer.play(AUDIO_GOOD_MORNING); Serial.println(">> הושמע: 'בוקר טוב'");
          } 
          else
           if (currentHour >= 12 && currentHour < 18) 
           {
            myDFPlayer.play(AUDIO_GOOD_AFTERNOON); Serial.println(">> הושמע: 'צהריים טובים'");
           } 
           else 
           if (currentHour >= 18 && currentHour < 22) 
           {
            myDFPlayer.play(AUDIO_GOOD_EVENING); Serial.println(">> הושמע: 'ערב טוב'");
           } 
           else 
           {
            myDFPlayer.play(AUDIO_GOOD_NIGHT); Serial.println(">> הושמע: 'לילה טוב'");
          }
          delay(2000);
        } 
        else 
        {
          String friendlyMessage;
          if (timeClient.getEpochTime() != 0)
          {
            int currentHour = timeClient.getHours();
            if (currentHour >= 5 && currentHour < 12) friendlyMessage = "בוקר טוב! מקווים שהכל בסדר איתך. שיהיה לך יום נהדר!";
            else if (currentHour >= 12 && currentHour < 18) friendlyMessage = "צהריים טובים! איך עובר היום? מקווים שאתה מרגיש טוב.";
            else if (currentHour >= 18 && currentHour < 22) friendlyMessage = "ערב טוב! מה שלומך? מקווים שאתה נהנה מהערב.";
            else friendlyMessage = "לילה טוב! מקווים שאתה נח היטב. שתהיה לך שינה ערבה!";
          } else {
            friendlyMessage = "שלום! ברוכים הבאים למערכת התרופות. עדיין לא סונכרן הזמן.";
          }
        }
      }
      Serial.println("------------------------------------");
      lastInteractionTime = millis();
    }
  }
  lastSensorValue = currentSensorValue;
}

// דגימת הטמפרטורה והלחות במיקום של התרופות ובדיקה האם עומדים בסף שהגדרתי 
void readAndCheckDHTSensor() {
  float humidity = dht.readHumidity(); // קריאת לחות
  float temperature = dht.readTemperature(); // קריאת טמפרטורה
  if (isnan (humidity) || isnan (temperature))
  {
    Serial.println("נכשל בקריאה מחיישן DHT!");
  }
  else
  {
    float heatIndex = dht.computeHeatIndex(temperature, humidity, false);
    Serial.print("לחות נוכחית: "); Serial.print(humidity); Serial.println("%");
    Serial.print("טמפרטורה נוכחית: "); Serial.print(temperature); Serial.println("°C");
    String tempMessage = "";
    if (temperature > MAX_TEMP_ALLOWED) 
    {
      tempMessage = "!!! התראה: טמפרטורה גבוהה מדי לתרופות! (" + String(temperature, 1) + "°C, מקסימום מותר: " + String(MAX_TEMP_ALLOWED) + "°C)";
    } 
    else if (temperature < MIN_TEMP_ALLOWED) 
    {
      tempMessage = "!!! התראה: טמפרטורה נמוכה מדי לתרופות! (" + String(temperature, 1) + "°C, מינימום מותר: " + String(MIN_TEMP_ALLOWED) + "°C)";
    } 
    else 
    {
      tempMessage = "טמפרטורה בטווח תקין.";
    }
    Serial.println(tempMessage);
    String humidityMessage = "";
    if (humidity > MAX_HUMIDITY_ALLOWED) 
    {
      humidityMessage = "!!! התראה: לחות גבוהה מדי לתרופות! (" + String(humidity, 1) + "%, מקסימום מותר: " + String(MAX_HUMIDITY_ALLOWED) + "%)";
    } 
    else if (humidity < MIN_HUMIDITY_ALLOWED) 
    {
      humidityMessage = "!!! התראה: לחות נמוכה מדי לתרופות! (" + String(humidity, 1) + "%, מינימום מותר: " + String(MIN_HUMIDITY_ALLOWED) + "%)";
    } 
    else 
    {
      humidityMessage = "לחות בטווח תקין.";
    }
    Serial.println(humidityMessage);
  }
}

//הםעלת הבקבוק המתאים
void dispenseDrug(int bottleNumber) 
{
  Serial.print("מנסה לשחרר תרופה מבקבוק מספר: ");
  Serial.println(bottleNumber);
  const int DISPENSE_ANGLE = 0;
  const int RESTING_ANGLE = 150;
  const int STEP_SIZE = 1;
  const int SENSOR_READ_DELAY_AFTER_RETURN = 200;
  const int SETTLE_AFTER_DETECTION = 200;
  const int MAX_DISPENSE_ATTEMPTS = 5;
  Servo* currentServo; // הצהרה על מצביע לסרוו
  switch (bottleNumber)
  {
    case 1: currentServo = &servoBottle1; break;
    case 2: currentServo = &servoBottle2; break;
    case 3: currentServo = &servoBottle3; break;
    default:
      Serial.println("שגיאה: מספר בקבוק לא חוקי צוין לשחרור!");
      return;
  }

  bool drugSuccessfullyDispensed = false;
  for (int attempt = 1; attempt <= MAX_DISPENSE_ATTEMPTS; attempt++)
  {
    Serial.print("ניסיון שחרור מספר #");
    Serial.println(attempt);
    int currentAngle = currentServo->read();
    if (currentAngle != RESTING_ANGLE) {
      Serial.print("מחזיר סרוו למצב מנוחה ("); Serial.print(RESTING_ANGLE); Serial.println(" מעלות) לפני ניסיון חדש.");
      if (RESTING_ANGLE > currentAngle) {
        for (int angle = currentAngle; angle <= RESTING_ANGLE; angle += STEP_SIZE) 
        {
          currentServo->write(angle);
          delay(STEP_DELAY);
        }
      } 
      else if (RESTING_ANGLE < currentAngle) 
      {
        for (int angle = currentAngle; angle >= RESTING_ANGLE; angle -= STEP_SIZE) 
        {
          currentServo->write(angle);
          delay(STEP_DELAY);
        }
      }
      delay(500);
    }
    currentAngle = currentServo->read();
    Serial.print("נע מזווית "); Serial.print(currentAngle); Serial.print(" לזווית "); Serial.println(DISPENSE_ANGLE);
    if (DISPENSE_ANGLE > currentAngle) 
    {
      for (int angle = currentAngle; angle <= DISPENSE_ANGLE; angle += STEP_SIZE) 
      {
        currentServo->write(angle);
        delay(STEP_DELAY); 
      }
    } 
    else if (DISPENSE_ANGLE < currentAngle) 
    {
      for (int angle = currentAngle; angle >= DISPENSE_ANGLE; angle -= STEP_SIZE) 
      {
        currentServo->write(angle);
        delay(STEP_DELAY); // שגיאה כאן, צריך להיות 25, אבל מניח שרצית STEP_DELAY
      }
    }
    delay(300);
    currentAngle = currentServo->read();
    Serial.print("נע מזווית "); Serial.print(currentAngle); Serial.print(" חזרה לזווית "); Serial.println(RESTING_ANGLE);
    if (RESTING_ANGLE > currentAngle) 
    {
      for (int angle = currentAngle; angle <= RESTING_ANGLE; angle += STEP_SIZE) 
      {
        currentServo->write(angle);
        delay(STEP_DELAY);
      }
    } 
    else if (RESTING_ANGLE < currentAngle) 
    {
      for (int angle = currentAngle; angle >= RESTING_ANGLE; angle -= STEP_SIZE) 
      {
        currentServo->write(angle);
        delay(STEP_DELAY);
      }
    }
    currentServo->write(RESTING_ANGLE);
    delay(FINAL_SETTLE_DELAY);
    Serial.print("ממתין "); Serial.print(SENSOR_READ_DELAY_AFTER_RETURN); Serial.println("ms לפני בדיקת חיישן...");
    delay(SENSOR_READ_DELAY_AFTER_RETURN);
    if (isDrugDetected(bottleNumber))
    {
      Serial.println("!!! תרופה זוהתה בהצלחה! מסיים ניסיונות. !!!");
      drugSuccessfullyDispensed = true;
      delay(SETTLE_AFTER_DETECTION);
      break;
    } 
    else
    {
      Serial.println("!!! לא זוהתה תרופה. מנסה שוב... !!!");
    }
  }

  if (!drugSuccessfullyDispensed) 
  {
    Serial.println("!!! נכשל בשחרור התרופה לאחר מספר ניסיונות. נדרשת בדיקה ידנית! !!!");
  }
}

// Handler מיוחד לבקשות OPTIONS (Preflight)
void handleOptions() 
{
  addCORSHeaders();
  server.send(204);
}


void setup()
{
  Serial.begin(115200);
  Serial.println("ESP32 Started!");

  // --- WiFi ---
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

  //  NTP Client 
  timeClient.begin();
  updateNTPTime(); 

  //  DHT11 חיישן טמפרטורה ולחות 
  dht.begin(); 

  //  חיישני מרחק IR 
  pinMode(GENERAL_IR_PROXIMITY_PIN, INPUT); //החיישן מלמעלה
  //שאר החיישנים
  pinMode(IRSensorPin1, INPUT);
  pinMode(IRSensorPin2, INPUT);
  pinMode(IRSensorPin3, INPUT);

  Serial.println("IR Proximity Sensor Test (Digital Output)");
  Serial.println("=========================================");
  Serial.println("Approaching an object to the sensor...");

  // חיישן דופק 
  pinMode(ECG_PIN, INPUT);

  //  חיישן חום גוף 
    Wire.begin(); 
  //  רמקול DFPlayer Mini 
  Serial.println("--- מתחיל בדיקת DFPlayer Mini ---");
  myDFPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN); // קצב 9600 ל-DFPlayer
  Serial.println("מנסה לאתחל DFPlayer Mini...");
 if (!myDFPlayer.begin(myDFPlayerSerial, true, false)) { // הוספתי true ו-false לאתחול. true לדיבוג, false ל-WaitUntilOnline
    Serial.println("שגיאה: DFPlayer Mini לא אותחל!");
    Serial.println("בדוק את הדברים הבאים:");
    Serial.println("1. חיווט: ודא ש-TX של ESP32 (GPIO2) מחובר ל-RX של DFPlayer, ו-RX של ESP32 (GPIO4) מחובר ל-TX של DFPlayer.");
    Serial.println("2. מתח: ודא ש-DFPlayer מקבל 5V יציבים ושיש חיבור אדמה משותף עם ה-ESP32.");
    Serial.println("3. כרטיס SD: ודא שכרטיס MicroSD בפורמט FAT32, מוכנס היטב, ויש עליו תיקיית 'mp3'.");
    Serial.println("4. קבצי אודיו: ודא שיש קבצי MP3 בתיקיית 'mp3' בשמות כמו 0001.mp3, 0002.mp3, וכו'.");
     while(1);

  }
  Serial.println("DFPlayer Mini אותחל בהצלחה!");
  dfPlayerReady = true; 
  myDFPlayer.volume(30);
  delay(1000);
  Serial.print("עוצמת שמע הוגדרה ל: ");
  Serial.println(myDFPlayer.readVolume());

  //  מנועי סרוו 
  Serial.println("אתחול מנועי סרוו...");
  servoBottle1.attach(SERVO1_PIN); 
  servoBottle2.attach(SERVO2_PIN);
  servoBottle3.attach(SERVO3_PIN);

  servoBottle1.write(0);
  servoBottle2.write(0);
  servoBottle3.write(0);

  //  Web Server (HTTP) 
  server.on("/update-drugs", HTTP_POST, handleUpdateDrugsSimple);
  server.on("/update-drugs", HTTP_OPTIONS, handleOptions);
  server.onNotFound([]() 
  {
    if (server.method() == HTTP_OPTIONS) {
      addCORSHeaders();
      server.send(204);
      Serial.println("התקבלה בקשת OPTIONS לא ידועה, טופלה על ידי onNotFound.");
    } else {
      addCORSHeaders();
      Serial.print("התקבלה בקשה לא ידועה: ");
      Serial.println(server.uri());
      Serial.print("שיטה: ");
      Serial.println(server.method() == HTTP_POST ? "POST" : (server.method() == HTTP_GET ? "GET" : (server.method() == HTTP_OPTIONS ? "OPTIONS" : "OTHER")));
      server.send(404, "text/plain", "לא נמצא (בדיקה פשוטה)");
    }
  });
  server.begin();
  Serial.println("Web Server Started.");
}


void loop()
{
  unsigned long currentMillis = millis(); 

  //  Web Server 
  server.handleClient(); 

  //  NTP Time Update 
  updateNTPTime();

  //  טמפרטורה ולחות
  static unsigned long lastDHTRead = 0;
  if (currentMillis - lastDHTRead >= 5000)
  { 
    readAndCheckDHTSensor();
    lastDHTRead = currentMillis;
  }


  //  דופק (ECG) 
  int ecgSum = 0;
  ecgSum += analogRead(ECG_PIN); 
  int ecgVal = ecgSum; 
  unsigned long now = millis();
  if (ecgVal > ECG_THRESHOLD && (now - lastECGPeak) > 500) 
  {
    Serial.print("ecgVal: "); 
    Serial.println(ecgVal);
    Serial.print("lastECGPeak: "); 
    Serial.println(lastECGPeak);
    bpm = 60000.0 / (now - lastECGPeak);
    lastECGPeak = now;
    normalizedBPM = constrain(bpm, 50.0, 120.0); 
    Serial.print("BPM: ");
    Serial.println(normalizedBPM);
  }


  //  חום גוף 
  static unsigned long lastTempRead = 0;
  if (currentMillis - lastTempRead >= 5000) 
  { 
    Wire.beginTransmission(TMP102_ADDR);
    Wire.write(0x00);  
    Wire.endTransmission(false); 
    if (Wire.requestFrom(TMP102_ADDR, 2) == 2) 
    {
      int16_t raw = (Wire.read() << 4) | (Wire.read() >> 4);
      float tempC = raw * 0.0625;
      Serial.print("טמפרטורת גוף: "); 
      Serial.print(tempC);
      Serial.println(" °C");
    }
    else
    {
      Serial.println("לא התקבלו נתונים מחיישן TMP102");
    }
    lastTempRead = currentMillis;
  }

  //  חיישן קרבה כללי 
  checkGeneralProximitySensor(); 

  //  בדיקת זמן ומתן תרופות 
  static unsigned long lastDrugCheckTime = 0;
  const long drugCheckInterval = 5000; 
  static int lastDay = -1; 
  if (currentMillis - lastDrugCheckTime > drugCheckInterval) 
  {
    lastDrugCheckTime = currentMillis;
    if (timeClient.getEpochTime() == 0) 
    { 
      Serial.println("הזמן לא סונכרן עדיין, מדלג על בדיקת תרופות.");
    } 
    else 
    { 
      String formattedTime = timeClient.getFormattedTime();
      String currentTime = formattedTime.substring(0, 5);  
      Serial.print("שעה נוכחית (HH:MM): ");
      Serial.println(currentTime);
      int currentDay = timeClient.getDay(); 
      if (lastDay == -1) 
      { 
        lastDay = currentDay;
      }
       else if (currentDay != lastDay) 
       { 
        Serial.println("זוהה יום חדש, מאפס דגלי שחרור עבור כל התרופות.");
        for (int i = 0; i < MAX_DRUGS; i++) {
          drugSchedule[i].dispensed = false;
        }
        lastDay = currentDay;
      }

      for (int i = 0; i < numDrugs; i++)
      {
        if (drugSchedule[i].time == currentTime && !drugSchedule[i].dispensed)
        {
          Serial.print("!!!!! הגיע זמן לשחרר ");
          Serial.print(drugSchedule[i].name);
          Serial.print(" מבקבוק ");
          Serial.print(drugSchedule[i].bottle);
          Serial.println(" !!!!!");
          if (dfPlayerReady) 
          { 
            myDFPlayer.play(AUDIO_PAY_ATTENTION); 
            delay(5000); 
          }
          else 
          {
            Serial.println("DFPlayer אינו מוכן, לא ניתן להשמיע התראה קולית.");
          }

          dispenseDrug(drugSchedule[i].bottle); 
          drugSchedule[i].dispensed = true; 
        }
      }
    }
  }
}