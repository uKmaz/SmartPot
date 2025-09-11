// Su Seviye Pini
const int WATER_LEVEL_GND_PIN = 32; // D32, Analog okuma 
const int WATER_LEVEL_10_PIN  = 33; // D33
const int WATER_LEVEL_25_PIN  = 25; // D25
const int WATER_LEVEL_50_PIN  = 27; // D27
const int WATER_LEVEL_75_PIN  = 14; // D14
const int WATER_LEVEL_100_PIN = 12; // D12


const int waterLevelPins[] = {WATER_LEVEL_10_PIN, WATER_LEVEL_25_PIN, WATER_LEVEL_50_PIN, WATER_LEVEL_75_PIN, WATER_LEVEL_100_PIN};
const int waterLevelValues[] = {10, 25, 50, 75, 100};
const int numWaterLevels = 5;


const int SOIL_MOISTURE_PIN = 34; 


const int PUMP_CONTROL_PIN = 22; // D22 (MOSFET Gate)

const int BATTERY_VOLTAGE_PIN = 35; // D35 
// Voltaj bölücüdeki direnç 33k ve 100k
const float R4_VALUE = 33000.0;
const float R5_VALUE = 100000.0;

//Ayarlar ve Eşik Değerleri 


const int MOISTURE_VERY_DRY = 2800; // Çok Kuru
const int MOISTURE_DRY      = 2200; // Kuru (Bu değerin altında sulama)
const int MOISTURE_WET      = 1500; // Nemli
const int MOISTURE_VERY_WET = 1000; // Çok Islak

// Su seviyesi için analog okuma eşiği (suya temas ettiğini anlamak için), su iletkenligine bagli
const int WATER_CONTACT_THRESHOLD = 2000; 


const unsigned long PUMP_RUN_DURATION = 10000; // 10 saniye

// --- Global Değişkenler ---
bool isPumping = false;              
unsigned long pumpStartTime = 0;      
int currentWaterLevel = 0;        


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("Sistem Başlatıldı!");

  // Su seviye pinlerini OUTPUT olarak 
  for (int i = 0; i < numWaterLevels; i++) {
    pinMode(waterLevelPins[i], OUTPUT);
  }

  // Pompa kontrol pinini OUTPUT olarak ayarla ve başlangıçta kapalı tut
  pinMode(PUMP_CONTROL_PIN, OUTPUT);
  digitalWrite(PUMP_CONTROL_PIN, LOW);


}

void loop() {
 
  checkWaterLevel();
  checkBatteryStatus();
  checkSoilAndManagePump();

  // 3. Adım: Pompa çalışıyorsa, durdurma zamanını kontrol et
  stopPumpIfNeeded();
  delay(2000); 
}

/**
 * @brief Su seviyesini ölçer ve kritik seviyede uyarı verir.
 */
void checkWaterLevel() {
  currentWaterLevel = 0; // Her ölçümden önce sıfırlan

  // Seviyeyi en üstten (%100) en alta doğru kontrol et.
  // Bulunan ilk seviye, suyun ulaştığı en yüksek noktadır.
  for (int i = numWaterLevels - 1; i >= 0; i--) {
    digitalWrite(waterLevelPins[i], HIGH);
    delay(10); // Sinyalin oturması için kısa bir bekleme
    int reading = analogRead(WATER_LEVEL_GND_PIN);
    digitalWrite(waterLevelPins[i], LOW);
    
    if (reading > WATER_CONTACT_THRESHOLD) {
      currentWaterLevel = waterLevelValues[i];
      break; // En yüksek seviye bulunduğunda döngüden çık
    }
  }

  Serial.println("--------------------");
  Serial.print("Mevcut Su Seviyesi: %");
  Serial.println(currentWaterLevel);

  // Su seviyesi %10 veya altındaysa uyarı ver
  if (currentWaterLevel <= 10 && currentWaterLevel > 0) {
    Serial.println("UYARI: Su seviyesi %10!");
  } else if (currentWaterLevel == 0) {
    Serial.println("UYARI: Depodaki su bitti!");
  }
}

void checkBatteryStatus() {
  // D35 pininden ham analog değeri oku (0-4095 arası)
  int adcValue = analogRead(BATTERY_VOLTAGE_PIN);

  // Ham değeri ESP32 pinindeki voltaja çevir
  float pinVoltage = (adcValue / 4095.0) * 3.3;

  
  // V_pil = V_pin * (R4 + R5) / R5
  float batteryVoltage = pinVoltage * (R4_VALUE + R5_VALUE) / R5_VALUE;

  // Pil voltajını yüzdeye çevir (Yaklaşık değerlerdir: 4.2V = %100, 3.2V = %0)
  float batteryPercentage = map(batteryVoltage * 100, 320, 420, 0, 100);
  if (batteryPercentage > 100) batteryPercentage = 100;
  if (batteryPercentage < 0) batteryPercentage = 0;


  Serial.print("Pil Voltajı: ");
  Serial.print(batteryVoltage);
  Serial.print("V (");
  Serial.print(batteryPercentage, 0); // Yüzdeyi ondalıksız göster
  Serial.println("%)");
}


/**
 * @brief Toprak nemini okur ve gerekirse sulama işlemini başlatır.
 */
void checkSoilAndManagePump() {
  int moistureValue = analogRead(SOIL_MOISTURE_PIN);
  
  
  Serial.print("Toprak Nem Değeri: ");
  Serial.print(moistureValue);
  Serial.print(" -> ");

  if (moistureValue >= MOISTURE_VERY_DRY) {
    Serial.println("Çok Kuru");
  } else if (moistureValue >= MOISTURE_DRY) {
    Serial.println("Kuru");
  } else if (moistureValue >= MOISTURE_WET) {
    Serial.println("Nemli");
  } else if (moistureValue >= MOISTURE_VERY_WET) {
    Serial.println("Islak");
  } else {
    Serial.println("Çok Islak");
  }

  
  if (moistureValue >= MOISTURE_DRY && !isPumping) {
    // Sulama yapmadan önce depoda su var mı diye kontrol et
    if (currentWaterLevel > 0) {
      Serial.println("Toprak kuru. Pompa 10 saniyeliğine çalıştırılıyor...");
      digitalWrite(PUMP_CONTROL_PIN, HIGH);
      isPumping = true;
      pumpStartTime = millis(); // Pompanın başladığı zamanı kaydet
    } else {
      Serial.println("Toprak kuru ancak depoda su yok! Pompa çalıştırılamadı.");
    }
  }
}

/**
 * @brief Pompa çalışıyorsa ve belirlenen süre dolduysa pompayı durdurur.
 */
void stopPumpIfNeeded() {
  // Sadece pompa çalışıyorsa bu kontrolü yap
  if (isPumping) {
    // Geçen süre, belirlenen çalışma süresini aştı mı kontrolu
    if (millis() - pumpStartTime >= PUMP_RUN_DURATION) {
      Serial.println("10 saniye doldu. Pompa durduruluyor.");
      digitalWrite(PUMP_CONTROL_PIN, LOW);
      isPumping = false;
    }
  }
}
