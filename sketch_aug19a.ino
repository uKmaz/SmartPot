#define BLYNK_TEMPLATE_ID "TMPL67BpjhRnR"
#define BLYNK_TEMPLATE_NAME "Akıllı Saksı"
#define BLYNK_AUTH_TOKEN "tb8zbZiyqdjO7XPH9x38j5jGUhRDeTyr"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>

BlynkTimer timer;

// --- Pinler ---
#define LED_PIN 2
#define RELAY_PIN 23   // Röle pini (su pompası)
#define SOIL_PIN 34    // Nem sensörü pini

// --- Değişkenler ---
bool pumpState = false;
bool manualControl = false;
int minMoisture = 30;
int soilMoisture = 0;

// --- Blynk Fonksiyonları ---
BLYNK_WRITE(V3) {  // Manuel pompa kontrolü
  manualControl = param.asInt();
  pumpState = manualControl;
  digitalWrite(RELAY_PIN, pumpState ? HIGH : LOW);
  digitalWrite(LED_PIN, pumpState ? HIGH : LOW);
  Blynk.virtualWrite(V2, pumpState ? 1 : 0);
}

BLYNK_WRITE(V4) {  // Minimum nem ayarı
  minMoisture = param.asInt();
  Serial.print("Yeni minimum nem eşiği: ");
  Serial.println(minMoisture);
}

// --- Sensör okuma ve otomatik pompa kontrol ---
void sendSensor() {
  int soilMoistureValue = analogRead(SOIL_PIN);
  soilMoisture = map(soilMoistureValue, 1200, 4095, 100, 0); 
  Blynk.virtualWrite(V1, soilMoisture);

  if (!manualControl) {
    pumpState = (soilMoisture < minMoisture);
    digitalWrite(RELAY_PIN, pumpState ? HIGH : LOW);
    digitalWrite(LED_PIN, pumpState ? HIGH : LOW);
    Blynk.virtualWrite(V2, pumpState ? 1 : 0);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(SOIL_PIN, INPUT);

  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN, LOW);

  // --- WiFiManager ile dinamik Wi-Fi bağlantısı ---
  WiFiManager wm;
  if (!wm.autoConnect("AkilliSaksi-Setup")) {
    Serial.println("WiFi bağlantısı başarısız, yeniden başlatılıyor...");
    ESP.restart();
  }
  Serial.println("WiFi bağlandı!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // --- Blynk bağlantısı ---
  Blynk.config(BLYNK_AUTH_TOKEN);
  if (!Blynk.connect()) {
    Serial.println("Blynk'e bağlanılamadı!");
  } else {
    Serial.println("Blynk'e bağlandı!");
  }

  // 5 saniyede bir sensör okuma
  timer.setInterval(5000L, sendSensor);
}

void loop() {
  Blynk.run();
  timer.run();
}
