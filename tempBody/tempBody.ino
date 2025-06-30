#include <Wire.h>

#define TMP102_ADDR 0x4C  // כתובת שנמצאה בסריקה

void setup() {
  Wire.begin();
  Serial.begin(115200);
}

void loop() {
  Wire.beginTransmission(TMP102_ADDR);
  Wire.write(0x00);  // רגיסטר טמפרטורה
  Wire.endTransmission();

  Wire.requestFrom(TMP102_ADDR, 2);
  if (Wire.available() == 2) {
    int16_t raw = (Wire.read() << 4) | (Wire.read() >> 4);
    float tempC = raw * 0.0625;
    Serial.print("טמפרטורה: ");
    Serial.print(tempC);
    Serial.println(" °C");
  } else {
    Serial.println("לא התקבלו נתונים מהחיישן");
  }

  delay(1000);
}
