#include <Adafruit_Sensor.h>
#include "DHT.h"

//לחבר לפין 26 השאר בעיתיים...
#define DHTTYPE DHT11 // סוג החיישן DHT11

DHT dht(26,DHTTYPE);

void setup() 
{
Serial.begin(9600); // התחלת תקשורת טורית
dht.begin(); // התחלת החיישן
}

void loop() 
{

delay(2000); // המתנה של 2 שנות בין קריאות

float humidity = dht.readHumidity(); // קריאת לחות

float temperature = dht.readTemperature(); // קריאת טמפרטורת בטלזיו

if (isnan (humidity) || isnan (temperature)) {
  Serial.println("נכשל בקריאה מחיישן DHT!");
return;

}

float heatIndex = dht.computeHeatIndex (temperature, humidity, false);
Serial.print("לחות: ");

Serial.println(humidity);

Serial.print("טמפרטורה באחוזים: ");

Serial.println(temperature);

}

