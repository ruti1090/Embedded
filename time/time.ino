// הגדרות NTP server
const long gmtOffset_sec = 3 * 3600; // GMT+3 (ישראל שעון קיץ)
const int daylightOffset_sec = 0;    // אין תוספת נוספת כי ה-gmtOffset_sec כבר כולל את ההפרש.
const char* ntpServer = "pool.ntp.org";

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer, gmtOffset_sec, daylightOffset_sec);

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