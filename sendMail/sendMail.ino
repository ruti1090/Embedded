#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <time.h>

// פרטי WiFi
const char* ssid = "";
const char* password = "";

// פרטי Gmail
#define SMTP_HOST "smtp.gmail.com"
#define SMTP_PORT 465
#define AUTHOR_EMAIL "R0548551090@gmail.com"
#define AUTHOR_PASSWORD ""
#define RECIPIENT_EMAIL "R0548551090@gmail.com"

// משתנה גלובלי שבו מצטברים נתונים מהחיישנים
extern String logData;  // או תגדירי פה אם הכל באותו קובץ
int lastSentDay = -1;

SMTPSession smtp;

void setup() {
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\n✅ WiFi מחובר");

  // הגדרת אזור זמן לישראל (UTC+3)
  configTime(10800, 0, "pool.ntp.org", "time.nist.gov");

  // בדיקת שהשעה התקבלה
  struct tm timeinfo;
  while (!getLocalTime(&timeinfo)) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n🕓 שעה סונכרנה");
}

void loop() {
  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    int hour = timeinfo.tm_hour;
    int minute = timeinfo.tm_min;
    int day = timeinfo.tm_mday;

    if (hour == 23 && minute == 30 && day != lastSentDay) {
      Serial.println("🕠 השעה 23:30 — שולח מייל");
      sendEmail(logData);
      lastSentDay = day;
      logData = "";  // אפס לאחר שליחה
    }
  }

  delay(10000); // בדיקה כל 10 שניות
}

void sendEmail(String message) {
  if (message.length() == 0) {
    Serial.println("⚠️ אין נתונים לשלוח, דילוג");
    return;
  }

  SMTP_Data smtpData;

  smtpData.setLogin(SMTP_HOST, SMTP_PORT, AUTHOR_EMAIL, AUTHOR_PASSWORD);
  smtpData.setSender("ESP32 Logger", AUTHOR_EMAIL);
  smtpData.setPriority("High");
  smtpData.setSubject("📋 דוח יומי – טמפרטורה ודופק");
  smtpData.setMessage(message, false);
  smtpData.addRecipient(RECIPIENT_EMAIL);

  Serial.println("📤 שולח מייל...");
  if (!MailClient.sendMail(&smtp, &smtpData)) {
    Serial.println("❌ שגיאת שליחה: " + smtp.errorReason());
  } else {
    Serial.println("✅ מייל נשלח בהצלחה!");
  }

  smtpData.empty();
}
