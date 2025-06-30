#define ECG_PIN 27  // שינוי מ-25 ל-34 (פין ADC)

const int ECG_THRESHOLD = 700;
unsigned long lastECGPeak = 0;
float bpm = 0;
float normalizedBPM = 77.0;

void setup() {
  Serial.begin(115200);  // הוספתי את זה
  pinMode(ECG_PIN, INPUT);
}

void loop() {
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
  
  // עיכוב כדי להאט את הלולאה
  delay(100);
}