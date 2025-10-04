// --- Blynk tanımları ---
#define BLYNK_TEMPLATE_ID "TMPL67BpjhRnR"
#define BLYNK_TEMPLATE_NAME "Akıllı Saksı"
#define BLYNK_AUTH_TOKEN "tb8zbZiyqdjO7XPH9x38j5jGUhRDeTyr"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>

// --- Su Seviye Pinleri ---
const int WATER_LEVEL_GND_PIN = 32; 
const int WATER_LEVEL_10_PIN  = 33;
const int WATER_LEVEL_25_PIN  = 25;
const int WATER_LEVEL_50_PIN  = 27;
const int WATER_LEVEL_75_PIN  = 14;
const int WATER_LEVEL_100_PIN = 12;

const int waterLevelPins[] = {WATER_LEVEL_10_PIN, WATER_LEVEL_25_PIN, WATER_LEVEL_50_PIN, WATER_LEVEL_75_PIN, WATER_LEVEL_100_PIN};
const int waterLevelValues[] = {10, 25, 50, 75, 100};
const int numWaterLevels = 5;

const int SOIL_MOISTURE_PIN = 34; 
const int PUMP_CONTROL_PIN = 22; 
const int BATTERY_VOLTAGE_PIN = 35;

// Voltaj bölücü dirençleri
const float R4_VALUE = 33000.0;
const float R5_VALUE = 100000.0;

// Nem değerleri
const int MOISTURE_VERY_DRY = 2800; 
const int MOISTURE_DRY      = 2200; 
const int MOISTURE_WET      = 1500; 
const int MOISTURE_VERY_WET = 1000; 

const int WATER_CONTACT_THRESHOLD = 2000; 
const unsigned long PUMP_RUN_DURATION = 10000; // 10 saniye

// --- Global değişkenler ---
bool isPumping = false;              
unsigned long pumpStartTime = 0;      
int currentWaterLevel = 0;        

// Deep sleep için
RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int sleepDuration = 10; // dakika

BlynkTimer timer;

// --- Fonksiyonlar ---
void checkWaterLevel() {
  currentWaterLevel = 0; 
  for (int i = numWaterLevels - 1; i >= 0; i--) {
    digitalWrite(waterLevelPins[i], HIGH);
    delay(10); 
    int reading = analogRead(WATER_LEVEL_GND_PIN);
    digitalWrite(waterLevelPins[i], LOW);
    
    if (reading > WATER_CONTACT_THRESHOLD) {
      currentWaterLevel = waterLevelValues[i];
      break; 
    }
  }

  Serial.print("Su Seviyesi: %");
  Serial.println(currentWaterLevel);

  Blynk.virtualWrite(V1, currentWaterLevel); // V1 → su seviyesi
}

void checkBatteryStatus() {
  int adcValue = analogRead(BATTERY_VOLTAGE_PIN);
  float pinVoltage = (adcValue / 4095.0) * 3.3;
  float batteryVoltage = pinVoltage * (R4_VALUE + R5_VALUE) / R5_VALUE;

  float batteryPercentage = map(batteryVoltage * 100, 320, 420, 0, 100);
  if (batteryPercentage > 100) batteryPercentage = 100;
  if (batteryPercentage < 0) batteryPercentage = 0;

  Serial.printf("Batarya: %.2fV (%d %%)\n", batteryVoltage, (int)batteryPercentage);
  Blynk.virtualWrite(V2, (int)batteryPercentage); // V2 → batarya %
}

void checkSoilAndManagePump() {
  int moistureValue = analogRead(SOIL_MOISTURE_PIN);
  Serial.print("Toprak Nem: ");
  Serial.println(moistureValue);

  int soilPercent = map(moistureValue, MOISTURE_VERY_DRY, MOISTURE_VERY_WET, 0, 100);
  soilPercent = constrain(soilPercent, 0, 100);
  Blynk.virtualWrite(V3, soilPercent); // V3 → nem %

  if (moistureValue >= MOISTURE_DRY && !isPumping) {
    if (currentWaterLevel > 0) {
      Serial.println("Toprak kuru → Pompa çalışıyor...");
      digitalWrite(PUMP_CONTROL_PIN, HIGH);
      isPumping = true;
      pumpStartTime = millis(); 
    } else {
      Serial.println("Toprak kuru ama depoda su yok!");
    }
  }
}

void stopPumpIfNeeded() {
  if (isPumping && millis() - pumpStartTime >= PUMP_RUN_DURATION) {
    Serial.println("Pompa durduruldu.");
    digitalWrite(PUMP_CONTROL_PIN, LOW);
    isPumping = false;
  }
}

void goToSleep() {
  Serial.println("Uykuya geçiliyor...");
  esp_sleep_enable_timer_wakeup((uint64_t)sleepDuration * 60 * 1000000ULL);
  esp_deep_sleep_start();
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Sistem Başlatıldı!");

  for (int i = 0; i < numWaterLevels; i++) {
    pinMode(waterLevelPins[i], OUTPUT);
  }
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  digitalWrite(PUMP_CONTROL_PIN, LOW);

  bootCount++;
  Serial.printf("Boot count: %d\n", bootCount);

  // WiFiManager
  WiFiManager wm;
  if (!wm.autoConnect("Saksi-Setup")) {
    Serial.println("WiFi bağlantısı başarısız, resetleniyor...");
    ESP.restart();
  }
  Serial.println("WiFi bağlandı!");

  // Blynk
  Blynk.config(BLYNK_AUTH_TOKEN);
  if (!Blynk.connect()) {
    Serial.println("Blynk bağlanamadı!");
  } else {
    Serial.println("Blynk bağlandı!");
  }
}

// --- Loop ---
void loop() {
  Blynk.run();

  checkWaterLevel();
  checkBatteryStatus();
  checkSoilAndManagePump();
  stopPumpIfNeeded();

  delay(10000);

  
  goToSleep();
}
