#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h> // וודא שספרייה זו מותקנת: ArduinoJson by Benoit Blanchon

// הגדרות WiFi - שנה/י זאת לפרטי הרשת שלך!
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
// זהו מערך דינמי שיכול להכיל עד 10 תרופות (ניתן לשנות את הגודל לפי הצורך)
Drug drugs[10];
int numDrugs = 0; // מונה התרופות הנוכחי ברשימה

// פונקציית עזר להדפסת רשימת התרופות הנוכחית
void printDrugs() {
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
void handleUpdateDrugs() {
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
  // גודל זה צריך להיות מספיק גדול כדי להכיל את כל ה-JSON
  // 1KB הוא בדרך כלל מספיק עבור מספר קטן של תרופות, ניתן להגדיל אם יש יותר נתונים
  StaticJsonDocument<1024> doc; 

  // ניתוח ה-JSON
  DeserializationError error = deserializeJson(doc, requestBody);

  if (error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    server.send(400, "text/plain", "Failed to parse JSON: " + String(error.f_str()));
    return;
  }

  // ניקוי רשימת התרופות הקיימת
  numDrugs = 0;

  // קריאת המערך "drugs" מה-JSON
  JsonArray drugsArray = doc["drugs"];

  if (drugsArray.isNull()) {
    Serial.println("Error: 'drugs' array not found in JSON.");
    server.send(400, "text/plain", "Bad Request: 'drugs' array missing.");
    return;
  }

  // מעבר על התרופות במערך ועדכון הרשימה הגלובלית
  for (JsonVariant v : drugsArray) {
    if (numDrugs < 10) { // לוודא שלא חורגים מגודל המערך
      drugs[numDrugs].name = v["name"].as<String>();
      drugs[numDrugs].time = v["time"].as<String>();
      drugs[numDrugs].bottle = v["bottle"].as<int>();
      numDrugs++;
    } else {
      Serial.println("Warning: Max drugs capacity reached. Some drugs might not be stored.");
      break; // הפסק אם הגענו למגבלה
    }
  }

  Serial.println("Drugs updated successfully!");
  printDrugs(); // הדפס את הרשימה המעודכנת

  server.send(200, "text/plain", "Drugs schedule updated successfully!");
}

void setup() {
  Serial.begin(115200); // הגדרת קצב התקשורת ב-Serial Monitor
  Serial.println("ESP32 Started!"); // הדפסה לוודא שהקוד התחיל

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
}

void loop() {
  server.handleClient(); // טיפול בבקשות HTTP נכנסות
}