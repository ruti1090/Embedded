#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ESP32Servo.h>
// Wire.h הוסר - לא נדרש יותר לאחר הסרת MAX30205

// --- ספריות וקונפיגורציה לחיישן DHT11 (טמפרטורה ולחות) ---
#include <Adafruit_Sensor.h>
#include "DHT.h"

#define DHTPIN 26
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

// הגדרת ספים מומלצים לטמפרטורה ולחות לתרופות
const float MAX_TEMP_ALLOWED = 25.0;
const float MIN_TEMP_ALLOWED = 15.0;
const float MAX_HUMIDITY_ALLOWED = 60.0;
const float MIN_HUMIDITY_ALLOWED = 30.0;

// --- סוף קונפיגורציית DHT ---

// --- קונפיגורציה עבור DFPlayer Mini ---
#include <HardwareSerial.h>      // נשתמש ב-HardwareSerial עבור ESP32
#include <DFRobotDFPlayerMini.h> // ספריית ה-DFPlayer Mini

// הגדרת פיני ה-UART עבור ה-DFPlayer Mini (GPIO2 ו-GPIO4)
// ESP32 GPIO2 יהיה ה-TX של ה-ESP32 (יתחבר ל-RX של ה-DFPlayer)
// ESP32 GPIO4 יהיה ה-RX של ה-ESP32 (יתחבר ל-TX של ה-DFPlayer)
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
// AUDIO_HIGH_TEMP_ALERT הוסרה לחלוטין
// --- סוף קונפיגורציית DFPlayer Mini ---




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

// הגדרת פיני GPIO לבקבוקים
const int BOTTLE_1_PIN = 18;
const int BOTTLE_2_PIN = 19;
const int BOTTLE_3_PIN = 13; // שימו לב: אם זה GPIO 13, יש לתקן כאן.

// הגדרת פיני GPIO לחיישני מרחק IR עבור הבקבוקים
const int IRSensorPin1 = 33;
const int IRSensorPin2 = 32;
const int IRSensorPin3 = 34;

// --- הגדרת פין לחיישן הקרבה הכללי (לא קשור לבקבוקים) ---
const int GENERAL_IR_PROXIMITY_PIN = 23;

// הגדרת אובייקטים של סרוו
Servo servoBottle1;
Servo servoBottle2;
Servo servoBottle3;

// פרטי רשת ה-WiFi שלך
const char* ssid = "";
const char* password = "";

WebServer server(80);

// הגדרות NTP server
const long gmtOffset_sec = 3 * 3600; // GMT+3 (ישראל שעון קיץ)
const int daylightOffset_sec = 0;    // אין תוספת נוספת כי ה-gmtOffset_sec כבר כולל את ההפרש.
const char* ntpServer = "pool.ntp.org";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

// פונקציית עזר להוספת כותרות CORS
void addCORSHeaders() {
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

// פונקציית עזר להפעלת הבקבוק המתאים (נותרה חוסמת)
void dispenseDrug(int bottleNumber) {
  Serial.print("מנסה לשחרר תרופה מבקבוק מספר: ");
  Serial.println(bottleNumber);

  const int DISPENSE_ANGLE = 30;
  const int RESTING_ANGLE = 180;
  const int STEP_SIZE = 1;
  const int STEP_DELAY = 25;
  const int HOLD_DISPENSE_DELAY = 300;
  const int FINAL_SETTLE_DELAY = 500;
  const int SENSOR_READ_DELAY_AFTER_RETURN = 200;
  const int SETTLE_AFTER_DETECTION = 200;
  const int MAX_DISPENSE_ATTEMPTS = 5;

  Servo* currentServo;

  switch (bottleNumber) {
    case 1: currentServo = &servoBottle1; break;
    case 2: currentServo = &servoBottle2; break;
    case 3: currentServo = &servoBottle3; break;
    default:
      Serial.println("שגיאה: מספר בקבוק לא חוקי צוין לשחרור!");
      return;
  }

  bool drugSuccessfullyDispensed = false;
  for (int attempt = 1; attempt <= MAX_DISPENSE_ATTEMPTS; attempt++) {
    Serial.print("ניסיון שחרור מספר #");
    Serial.println(attempt);

    int currentAngle = currentServo->read();
    if (currentAngle != RESTING_ANGLE) {
      Serial.print("מחזיר סרוו למצב מנוחה ("); Serial.print(RESTING_ANGLE); Serial.println(" מעלות) לפני ניסיון חדש.");
      if (RESTING_ANGLE > currentAngle) {
        for (int angle = currentAngle; angle <= RESTING_ANGLE; angle += STEP_SIZE) {
          currentServo->write(angle);
          delay(STEP_DELAY);
        }
      } else if (RESTING_ANGLE < currentAngle) {
        for (int angle = currentAngle; angle >= RESTING_ANGLE; angle -= STEP_SIZE) {
          currentServo->write(angle);
          delay(STEP_DELAY);
        }
      }
      delay(FINAL_SETTLE_DELAY);
    }

    currentAngle = currentServo->read();
    Serial.print("נע מזווית "); Serial.print(currentAngle); Serial.print(" לזווית "); Serial.println(DISPENSE_ANGLE);

    if (DISPENSE_ANGLE > currentAngle) {
      for (int angle = currentAngle; angle <= DISPENSE_ANGLE; angle += STEP_SIZE) {
        currentServo->write(angle);
        delay(STEP_DELAY);
      }
    } else if (DISPENSE_ANGLE < currentAngle) {
      for (int angle = currentAngle; angle >= DISPENSE_ANGLE; angle -= STEP_SIZE) {
        currentServo->write(angle);
        delay(STEP_DELAY);
      }
    }

    delay(HOLD_DISPENSE_DELAY);

    currentAngle = currentServo->read();
    Serial.print("נע מזווית "); Serial.print(currentAngle); Serial.print(" חזרה לזווית "); Serial.println(RESTING_ANGLE);

    if (RESTING_ANGLE > currentAngle) {
      for (int angle = currentAngle; angle <= RESTING_ANGLE; angle += STEP_SIZE) {
        currentServo->write(angle);
        delay(STEP_DELAY);
      }
    } else if (RESTING_ANGLE < currentAngle) {
      for (int angle = currentAngle; angle >= RESTING_ANGLE; angle -= STEP_SIZE) {
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
    } else 
    {
      Serial.println("!!! לא זוהתה תרופה. מנסה שוב... !!!");
    }
  }

  if (!drugSuccessfullyDispensed) {
    Serial.println("!!! נכשל בשחרור התרופה לאחר מספר ניסיונות. נדרשת בדיקה ידנית! !!!");
  }
}

// Handler לבקשות POST ל-/update-drugs
void handleUpdateDrugsSimple() {
  addCORSHeaders();

  Serial.println("!!!!!!! קיבלתי בקשת POST ל-/update-drugs - הצלחה !!!!!!!");

  if (server.hasArg("plain")) {
    String requestBody = server.arg("plain");
    Serial.print("אורך גוף הבקשה שהתקבל: ");
    Serial.println(requestBody.length());
    Serial.print("תוכן גוף הבקשה שהתקבל: ");
    Serial.println(requestBody);

    StaticJsonDocument<1024> doc; // הגדלתי ל-1024 ליתר ביטחון
    DeserializationError error = deserializeJson(doc, requestBody);

    if (error) {
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

    if (!drugsArray) {
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

    for (JsonObject drug : drugsArray) {
      if (numDrugs < MAX_DRUGS) {
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
      } else {
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

  } else {
    Serial.println("לא התקבל גוף בבקשת POST.");
    StaticJsonDocument<100> responseDoc;
    responseDoc["status"] = "error";
    responseDoc["message"] = "Bad request: No body received.";
    String jsonResponse;
    serializeJson(responseDoc, jsonResponse);
    server.send(400, "application/json", jsonResponse);
  }
}

// Handler מיוחד לבקשות OPTIONS (Preflight)
void handleOptions() {
  addCORSHeaders();
  server.send(204);
}

// פונקציה חדשה לעדכון וטיפול בזמן NTP
void updateNTPTime() {
  static unsigned long lastNTPUpdate = 0;
  const long ntpUpdateInterval = 15 * 60 * 1000; // 15 דקות במקום 5, כדי לא להציף.

  // וודא חיבור WiFi לפני ניסיון עדכון NTP
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi לא מחובר, לא ניתן לעדכן NTP כעת. סטטוס WiFi: " + String(WiFi.status()));
    return;
  }

  // עדכן NTP רק אם עבר מספיק זמן או בפעם הראשונה
  if (millis() - lastNTPUpdate >= ntpUpdateInterval) {
    Serial.println("מתחיל עדכון זמן NTP...");
    if (timeClient.update()) {
      Serial.print("הזמן סונכרן בהצלחה: ");
      Serial.println(timeClient.getFormattedTime());
      Serial.print("Epoch Time (UTC): ");
      Serial.println(timeClient.getEpochTime());
      Serial.print("יום בשבוע: ");
      Serial.println(timeClient.getDay());
    } else {
      Serial.println("נכשל עדכון זמן NTP.");
    }
    lastNTPUpdate = millis();
  }
}

// פונקציה לקריאת טמפרטורה ולחות והדפסה ל-Serial Monitor
void readAndCheckDHTSensor() {
  static unsigned long lastDHTRead = 0;
  const long dhtReadInterval = 10000; // 10 שניות

  if (millis() - lastDHTRead >= dhtReadInterval) {
    lastDHTRead = millis();

    float humidity = dht.readHumidity();
    float temperature = dht.readTemperature();

    if (isnan(humidity) || isnan(temperature)) {
      Serial.println("שגיאה בקריאה מחיישן DHT11! אנא ודא/י שהחיישן מחובר כראוי.");
      return;
    }

    Serial.print("לחות נוכחית: "); Serial.print(humidity); Serial.println("%");
    Serial.print("טמפרטורה נוכחית: "); Serial.print(temperature); Serial.println("°C");

    String tempMessage = "";
    if (temperature > MAX_TEMP_ALLOWED) {
      tempMessage = "!!! התראה: טמפרטורה גבוהה מדי לתרופות! (" + String(temperature, 1) + "°C, מקסימום מותר: " + String(MAX_TEMP_ALLOWED) + "°C)";
    } else if (temperature < MIN_TEMP_ALLOWED) {
      tempMessage = "!!! התראה: טמפרטורה נמוכה מדי לתרופות! (" + String(temperature, 1) + "°C, מינימום מותר: " + String(MIN_TEMP_ALLOWED) + "°C)";
    } else {
      tempMessage = "טמפרטורה בטווח תקין.";
    }
    Serial.println(tempMessage);

    String humidityMessage = "";
    if (humidity > MAX_HUMIDITY_ALLOWED) {
      humidityMessage = "!!! התראה: לחות גבוהה מדי לתרופות! (" + String(humidity, 1) + "%, מקסימום מותר: " + String(MAX_HUMIDITY_ALLOWED) + "%)";
    } else if (humidity < MIN_HUMIDITY_ALLOWED) {
      humidityMessage = "!!! התראה: לחות נמוכה מדי לתרופות! (" + String(humidity, 1) + "%, מינימום מותר: " + String(MIN_HUMIDITY_ALLOWED) + "%)"; // תיקון שגיאה כאן
    } else {
      humidityMessage = "לחות בטווח תקין.";
    }
    Serial.println(humidityMessage);

    Serial.println("------------------------------------");
  }
}

// פונקציה לקריאת חיישן טמפרטורת גוף MAX30205 - הוסרה לחלוטין
// void readBodyTemperatureSensor() { ... }


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

      bool foundDrugOrAlert = false; // דגל כדי לדעת אם הושמעה כבר הודעה כלשהי

      // בדיקה אם יש תרופה ממתינה (אין בדיקת חום גוף)
      if (timeClient.getEpochTime() != 0) { // נבדוק רק אם הזמן סונכרן
        String currentTime = timeClient.getFormattedTime().substring(0, 5); // HH:MM

        for (int bottleNum = 1; bottleNum <= 3; bottleNum++) 
        {
          bool isDrugPending = false;
          String pendingDrugName = "לא ידועה";
          
          for (int i = 0; i < numDrugs; i++) 
          {
            if (drugSchedule[i].bottle == bottleNum) {
              if (currentTime.compareTo(drugSchedule[i].time) >= 0 && !drugSchedule[i].dispensed) {
                  isDrugPending = true;
                  pendingDrugName = drugSchedule[i].name;
                  break;
              }
            }
          }
          
          if (isDrugPending) {
            if (dfPlayerReady) {
              myDFPlayer.play(0005); 
              Serial.print(">> הושמע: 'שים לב! תרופת ה-"); Serial.print(pendingDrugName);
              Serial.print(" מבקבוק "); Serial.print(bottleNum); Serial.println(" ממתינה שתקח אותה.'");
              delay(2000); 
            } else {
              Serial.print(">> שים לב! תרופת ה-"); Serial.print(pendingDrugName);
              Serial.print(" מבקבוק "); Serial.print(bottleNum);
              Serial.println(" ממתינה שתקח אותה. אנא ודא/י את הסטטוס שלה.");
            }
            foundDrugOrAlert = true; // סמן שהושמעה הודעה/התראה
            break; // צא מלולאת הבקבוקים אחרי שמצאנו תרופה אחת שממתינה
          } else if (isDrugDetected(bottleNum)) { // אם אין תרופה מתוזמנת, אבל חיישן IR מזהה משהו
            String drugName = "לא ידועה";
            for (int i = 0; i < numDrugs; i++) {
              if (drugSchedule[i].bottle == bottleNum) {
                drugName = drugSchedule[i].name;
                break;
              }
            }
            // אם אין תרופה מתוזמנת, וגם אין הודעה קולית ייעודית לכך, נדפיס ל-Serial
            Serial.print(">> שימו לב, נראה שיש פריט בבקבוק "); Serial.print(bottleNum);
            Serial.print(" (אולי "); Serial.print(drugName); Serial.println(").");
            foundDrugOrAlert = true; // סמן שהושמעה הודעה/התראה
            break; // צא מלולאת הבקבוקים
          }
        }
      } else { // אם הזמן לא סונכרן עדיין
          Serial.println("הזמן לא סונכרן עדיין, לא ניתן לבדוק תרופות ממתינות.");
      }

      // השמעת הודעות "בוקר טוב", "צהריים טובים" וכו' (רק אם לא הושמעה כבר הודעה/התראה)
      if (!foundDrugOrAlert) {
        if (dfPlayerReady && timeClient.getEpochTime() != 0) { // השתמש בזמן רק אם הוא סונכרן
            int currentHour = timeClient.getHours();
            if (currentHour >= 5 && currentHour < 12) {
                myDFPlayer.play(0003); Serial.println(">> הושמע: 'בוקר טוב'");
            } else if (currentHour >= 12 && currentHour < 18) {
                myDFPlayer.play(0004); Serial.println(">> הושמע: 'צהריים טובים'");
            } else if (currentHour >= 18 && currentHour < 22) {
                myDFPlayer.play(0002); Serial.println(">> הושמע: 'ערב טוב'");
            } else {
                myDFPlayer.play(0001); Serial.println(">> הושמע: 'לילה טוב'");
            }
            delay(2000); 
        } else {
            // אם ה-DFPlayer לא מוכן, או שהזמן לא סונכרן, נמשיך להדפיס ל-Serial Monitor
            String friendlyMessage;
            if (timeClient.getEpochTime() != 0) {
                int currentHour = timeClient.getHours();
                if (currentHour >= 5 && currentHour < 12) friendlyMessage = "בוקר טוב! מקווים שהכל בסדר איתך. שיהיה לך יום נהדר!";
                else if (currentHour >= 12 && currentHour < 18) friendlyMessage = "צהריים טובים! איך עובר היום? מקווים שאתה מרגיש טוב.";
                else if (currentHour >= 18 && currentHour < 22) friendlyMessage = "ערב טוב! מה שלומך? מקווים שאתה נהנה מהערב.";
                else friendlyMessage = "לילה טוב! מקווים שאתה נח היטב. שתהיה לך שינה ערבה!";
            } else {
                friendlyMessage = "שלום! ברוכים הבאים למערכת התרופות. עדיין לא סונכרן הזמן.";
            }
            // Serial.println(">> מה נשמע? " + friendlyMessage);
        }
      }
      Serial.println("------------------------------------");
      lastInteractionTime = millis();
    }
  }
  lastSensorValue = currentSensorValue;
}

void setup() {
  Serial.begin(115200);
  Serial.println("בדיקת ESP32 פשוטה הופעלה!");

  pinMode(IRSensorPin1, INPUT);
  pinMode(IRSensorPin2, INPUT);
  pinMode(IRSensorPin3, INPUT);
  pinMode(GENERAL_IR_PROXIMITY_PIN, INPUT);
  Serial.print("חיישן קרבה כללי בפין "); Serial.print(GENERAL_IR_PROXIMITY_PIN); Serial.println(" אותחל.");

  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);

  servoBottle1.attach(BOTTLE_1_PIN);
  servoBottle2.attach(BOTTLE_2_PIN);
  servoBottle3.attach(BOTTLE_3_PIN);

  servoBottle1.write(180);
  servoBottle2.write(180);
  servoBottle3.write(180);
  delay(500);

  dht.begin();
  Serial.println("חיישן DHT11 אותחל.");

  // --- אתחול DFPlayer Mini ---
  myDFPlayerSerial.begin(9600, SERIAL_8N1, DFPLAYER_RX_PIN, DFPLAYER_TX_PIN);
  Serial.println("מאתחל DFPlayer...");
  if (!myDFPlayer.begin(myDFPlayerSerial)) 
  {
    Serial.println("שגיאה: DFPlayer לא התחיל. בדוק חיווט, מתח, וכרטיס SD.");
    Serial.println("וודא שקבצי המוזיקה נמצאים בתיקייה 'mp3' בכרטיס ה-SD ושהם בעלי שמות כמו 0001.mp3, 0002.mp3.");
    dfPlayerReady = false; // סמן שה-DFPlayer אינו מוכן
  } else 
  {
    Serial.println("DFPlayer מוכן");
    myDFPlayer.volume(30); // הגדר עוצמת שמע מקסימלית (0-30)
    Serial.println("עוצמת שמע הוגדרה ל-30");
    dfPlayerReady = true; // סמן שה-DFPlayer מוכן
  }
  // --- סוף אתחול DFPlayer Mini ---

  // --- אתחול חיישן טמפרטורת גוף MAX30205 ---
  // הקוד הבא הוסר לחלוטין מכיוון שאינו נדרש
  // Wire.begin();
  // Serial.println("מאתחל חיישן טמפרטורת גוף MAX30205...");
  // if (!bodyTempSensor.begin()) {
  //   Serial.println("שגיאה: חיישן MAX30205 לא אותחל! וודא חיווט נכון (SDA: GPIO21, SCL: GPIO22) והוא מחובר למתח.");
  // } else {
  //   Serial.println("חיישן MAX30205 אותחל בהצלחה.");
  // }
  // --- סוף אתחול חיישן MAX30205 ---


  // הגדרת שרתי DNS ידנית (מומלץ לאמינות NTP)
  WiFi.setDNS(IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));
  Serial.println("הוגדרו שרתי DNS של גוגל.");

  Serial.print("מתחבר ל-WiFi: "); Serial.println(ssid);
  WiFi.begin(ssid, password);

  unsigned long wifiConnectStartTime = millis();
  const long WIFI_CONNECT_TIMEOUT = 30000; // 30 שניות לחיבור WiFi

  while (WiFi.status() != WL_CONNECTED && millis() - wifiConnectStartTime < WIFI_CONNECT_TIMEOUT) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi מחובר בהצלחה.");
    Serial.print("כתובת IP: ");
    Serial.println(WiFi.localIP());

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

    if (!ntpSynced) {
      Serial.println("!!! נכשל סנכרון NTP ראשוני ב-setup לאחר מספר ניסיונות. ימשיך לנסות ב-loop. !!!");
    }

  } else {
    Serial.println("נכשל חיבור ל-WiFi. אנא בדוק SSID/סיסמה ורשת.");
  }

  server.on("/update-drugs", HTTP_POST, handleUpdateDrugsSimple);
  server.on("/update-drugs", HTTP_OPTIONS, handleOptions);

  server.onNotFound([]() {
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
  Serial.println("שרת HTTP הופעל (בדיקה פשוטה)");
}

void loop() {

  unsigned long currentMillis = millis();
  static unsigned long lastLoopActionTime = 0;
  const long MIN_LOOP_CYCLE_TIME = 50; // הגדלתי ל-50ms כדי לאפשר למעבד יותר זמן

  server.handleClient(); // טיפול בבקשות HTTP (תמיד חשוב שזה ירוץ)

  // בודק אם עבר מספיק זמן כדי לבצע את מחזור הפעולות הראשי
  if (currentMillis - lastLoopActionTime >= MIN_LOOP_CYCLE_TIME) {
    lastLoopActionTime = currentMillis;

    // 1. עדכון זמן NTP באופן תקופתי
    updateNTPTime();

    // 2. קריאת חיישן DHT11 והצגת התראות
    readAndCheckDHTSensor();

    // פונקציית חיישן חום גוף הוסרה מכאן
    // readBodyTemperatureSensor(); 

    // 4. בדיקה של חיישן הקרבה הכללי ותגובה (לא יפעיל יותר את ה-DFPlayer בהתאם לטמפרטורת גוף)
    checkGeneralProximitySensor(); 

    // 5. בדיקת זמן ומתן תרופות
    static unsigned long lastDrugCheckTime = 0;
    const long drugCheckInterval = 5000; // בדוק תרופות כל 5 שניות
    static int lastDay = -1; // משתנה לעקוב אחר היום הנוכחי לאיפוס דגלי שחרור

    if (currentMillis - lastDrugCheckTime > drugCheckInterval) {
      lastDrugCheckTime = currentMillis;

      // וודא שהזמן של ה-NTP תקין לפני השימוש
      if (timeClient.getEpochTime() == 0) { // אם Epoch Time הוא 0, כנראה הזמן לא סונכרן
          Serial.println("הזמן לא סונכרן עדיין, מדלג על בדיקת תרופות.");
          return; // אל תמשיך אם הזמן לא תקין
      }
      
      String formattedTime = timeClient.getFormattedTime(); // HH:MM:SS
      String currentTime = formattedTime.substring(0, 5);  // HH:MM

      Serial.print("שעה נוכחית (HH:MM): ");
      Serial.println(currentTime);

      int currentDay = timeClient.getDay(); // יום בשבוע (0-6, ראשון=0)
      if (lastDay == -1) { // אתחול בפעם הראשונה
          lastDay = currentDay;
      } else if (currentDay != lastDay) { // זוהה יום חדש
        Serial.println("זוהה יום חדש, מאפס דגלי שחרור עבור כל התרופות.");
        for (int i = 0; i < MAX_DRUGS; i++) {
          drugSchedule[i].dispensed = false;
        }
        lastDay = currentDay;
      }

      for (int i = 0; i < numDrugs; i++) {
        // בדוק אם הזמן הנוכחי תואם לשעת התרופה, וטרם שוחררה היום
        if (drugSchedule[i].time == currentTime && !drugSchedule[i].dispensed) 
        {
          Serial.print("!!!!! הגיע זמן לשחרר ");
          Serial.print(drugSchedule[i].name);
          Serial.print(" מבקבוק ");
          Serial.print(drugSchedule[i].bottle);
          Serial.println(" !!!!!");
          // אם ה-DFPlayer מוכן, נשמיע הודעה קולית
          if (dfPlayerReady) {
              myDFPlayer.play(0005); // קובץ 0005.mp3 - "שים לב"
              delay(2000); // המתן קצת שההודעה תסתיים
          } else {
              Serial.println(">>> התראה קולית לא הושמעה (DFPlayer אינו מוכן).");
          }

          dispenseDrug(drugSchedule[i].bottle); // קורא לפונקציה החוסמת
          drugSchedule[i].dispensed = true; // סמן כששוחרר
        }
      }
    }
  }
}
