#define BLYNK_PRINT Serial
#include "secrets.h"

#define BLYNK_TEMPLATE_ID   SECRET_BLYNK_TEMPLATE_ID
#define BLYNK_TEMPLATE_NAME SECRET_BLYNK_TEMPLATE_NAME
#define BLYNK_AUTH_TOKEN    SECRET_BLYNK_AUTH_TOKEN

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>

// ==================== NETWORK CREDENTIALS ====================
char ssid[] = SECRET_WIFI_SSID;
char pass[] = SECRET_WIFI_PASS;
// ==================== PIN DEFINITIONS ====================
const int MQ2_DO_PIN    = 32;  
const int FLAME_AO_PIN  = 33;  

const int GREEN_LED_PIN = 22;  
const int RED_LED_PIN   = 23;  
const int BUZZER_PIN    = 21;  

// DHT22 & Fan Pins
#define DHTPIN 4               // GPIO 4 for DHT22
#define DHTTYPE DHT22     
#define FAN_PIN 5              // GPIO 5 for 2N2222 Transistor Base

// ==================== THRESHOLDS ====================
const int FLAME_THRESHOLD  = 2000; 
const float TEMP_THRESHOLD = 38.0; // °C (Adapted for Bangladesh indoor climate)
const float HUM_THRESHOLD  = 85.0; // %  (Adapted for Bangladesh humidity)

DHT dht(DHTPIN, DHTTYPE);
BlynkTimer timer;

// System State Flags
bool buzzerState    = false;
bool buzzerMuted    = false; // V6 Switch
bool manualFanState = false; // V7 Switch
bool isTesting      = false; // V8 Button Diagnostic Lock

// Push Notification Cooldown Timers (Prevents spamming Blynk API)
unsigned long lastGasAlertTime  = 0;
unsigned long lastFireAlertTime = 0;
const unsigned long NOTIFY_COOLDOWN = 60000; // 60-second limit between push alerts

// ==================== SMART SAFETY ANALYSIS ====================
void calculateSafetyAnalysis(
  bool gasDetected,
  bool flameDetected,
  bool tempHigh,
  bool humHigh,
  int &safetyScore,
  String &riskLevel,
  String &recommendation
) {
  safetyScore = 100;

  if (gasDetected)   safetyScore -= 40;
  if (flameDetected) safetyScore -= 60;
  if (tempHigh)      safetyScore -= 15;
  if (humHigh)       safetyScore -= 10;

  if (safetyScore < 0) safetyScore = 0;

  if (gasDetected && flameDetected) {
    riskLevel = "CRITICAL";
    recommendation = "FIRE AND GAS HAZARD! EVACUATE / CHECK KITCHEN IMMEDIATELY.";
  } else if (flameDetected) {
    riskLevel = "CRITICAL";
    recommendation = "FLAME DETECTED! CHECK KITCHEN IMMEDIATELY.";
  } else if (gasDetected) {
    riskLevel = "HIGH RISK";
    recommendation = "POSSIBLE GAS LEAK. TURN OFF GAS AND VENTILATE.";
  } else if (tempHigh && humHigh) {
    riskLevel = "CAUTION";
    recommendation = "HIGH TEMPERATURE & HUMIDITY. VENTILATION ADVISED.";
  } else if (tempHigh) {
    riskLevel = "CAUTION";
    recommendation = "HIGH TEMPERATURE DETECTED. EXHAUST FAN ACTIVATED.";
  } else if (humHigh) {
    riskLevel = "CAUTION";
    recommendation = "HIGH HUMIDITY DETECTED. IMPROVE KITCHEN VENTILATION.";
  } else if (safetyScore >= 80) {
    riskLevel = "SAFE";
    recommendation = "KITCHEN CONDITIONS ARE NORMAL.";
  } else if (safetyScore >= 60) {
    riskLevel = "CAUTION";
    recommendation = "MINOR ENVIRONMENTAL WARNING. MONITOR CONDITIONS.";
  } else if (safetyScore >= 30) {
    riskLevel = "HIGH RISK";
    recommendation = "UNSAFE CONDITION DETECTED. CHECK THE KITCHEN.";
  } else {
    riskLevel = "CRITICAL";
    recommendation = "CRITICAL CONDITION. CHECK KITCHEN IMMEDIATELY.";
  }
}

// ==================== BLYNK HANDLERS ====================

// V6: Silence / Acknowledge Switch
BLYNK_WRITE(V6) {
  buzzerMuted = (param.asInt() == 1);
}

// V7: Manual Fan Override Switch
BLYNK_WRITE(V7) {
  manualFanState = (param.asInt() == 1);
}

// V8: Emergency Hardware Diagnostic Test Button
BLYNK_WRITE(V8) {
  if (param.asInt() == 1 && !isTesting) {
    isTesting = true;
    Serial.println("[DIAGNOSTICS] Starting 2-second hardware test...");
    
    if (Blynk.connected()) {
      Blynk.virtualWrite(V2, "HARDWARE TEST RUNNING...");
    }

    // Energize outputs simultaneously
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, HIGH);
    digitalWrite(BUZZER_PIN, HIGH);
    digitalWrite(FAN_PIN, HIGH);

    delay(2000); // Duration of test

    // Restore safe default states
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    digitalWrite(FAN_PIN, LOW);

    isTesting = false;
    Serial.println("[DIAGNOSTICS] Hardware test complete.");
  }
}

// ==================== SENSOR & CONTROL ROUTINE ====================
void sendSensorData() {
  if (isTesting) return; // Pause readings while running hardware test

  // 1. Read Safety Sensors
  int mq2State = digitalRead(MQ2_DO_PIN);
  bool gasDetected = (mq2State == LOW);

  int flameVal = analogRead(FLAME_AO_PIN);
  bool flameDetected = (flameVal < FLAME_THRESHOLD);

  // 2. Read DHT22
  float t = dht.readTemperature();
  float h = dht.readHumidity();

  bool tempHigh = (!isnan(t) && t > TEMP_THRESHOLD);
  bool humHigh  = (!isnan(h) && h > HUM_THRESHOLD);

  // 3. Safety Hardware Outputs (LEDs & Buzzer)
  if (gasDetected || flameDetected) {
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    
    if (!buzzerMuted) {
      buzzerState = !buzzerState;
      digitalWrite(BUZZER_PIN, buzzerState ? HIGH : LOW);
    } else {
      digitalWrite(BUZZER_PIN, LOW); // Silenced via Blynk
    }
  } else {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    digitalWrite(BUZZER_PIN, LOW);
    buzzerState = false;
    
    // Reset silence switch when hazards clear
    if (buzzerMuted) {
      buzzerMuted = false;
      if (Blynk.connected()) Blynk.virtualWrite(V6, 0);
    }
  }

  // 4. Exhaust Fan Control (Gas, Temp, Hum, OR Manual V7 Switch)
  bool fanShouldRun = (gasDetected || tempHigh || humHigh || manualFanState);
  digitalWrite(FAN_PIN, fanShouldRun ? HIGH : LOW);

  // 5. Smart Safety Analysis Calculation
  int safetyScore;
  String riskLevel;
  String recommendation;

  calculateSafetyAnalysis(
    gasDetected, flameDetected, tempHigh, humHigh,
    safetyScore, riskLevel, recommendation
  );

  // 6. Blynk IoT Dashboard & Notification Updates
  if (Blynk.connected()) {
    // Basic Sensors
    Blynk.virtualWrite(V0, gasDetected ? "GAS LEAK!" : "CLEAR");
    
    int blynkFlameVal = map(flameVal, 0, 4095, 4095, 0);
    Blynk.virtualWrite(V1, blynkFlameVal);

    if (gasDetected && flameDetected) {
      Blynk.virtualWrite(V2, "CRITICAL: FIRE & GAS!");
    } else if (gasDetected) {
      Blynk.virtualWrite(V2, "CRITICAL: GAS LEAK!");
    } else if (flameDetected) {
      Blynk.virtualWrite(V2, "CRITICAL: FIRE DETECTED!");
    } else {
      Blynk.virtualWrite(V2, "SAFE");
    }

    if (!isnan(t)) Blynk.virtualWrite(V3, t);
    if (!isnan(h)) Blynk.virtualWrite(V4, h);

    Blynk.virtualWrite(V5, fanShouldRun ? 1 : 0);

    // Smart Analysis Virtual Pins
    Blynk.virtualWrite(V9, safetyScore);
    Blynk.virtualWrite(V10, riskLevel);
    Blynk.virtualWrite(V11, recommendation);

    // Push Notifications (Rate limited to avoid spam)
    if (gasDetected && (millis() - lastGasAlertTime > NOTIFY_COOLDOWN)) {
      Blynk.logEvent("gas_alert", "WARNING: Gas leak detected in kitchen!");
      lastGasAlertTime = millis();
    }

    if (flameDetected && (millis() - lastFireAlertTime > NOTIFY_COOLDOWN)) {
      Blynk.logEvent("fire_alert", "CRITICAL: Fire/Flame detected in kitchen!");
      lastFireAlertTime = millis();
    }
  }

  // Serial Monitor Debug Output
  Serial.print("MQ-2: ");
  Serial.print(gasDetected ? "GAS" : "CLEAR");
  Serial.print(" | Flame: ");
  Serial.print(flameVal);
  if (!isnan(t) && !isnan(h)) {
    Serial.print(" | Temp: "); Serial.print(t); Serial.print("C");
    Serial.print(" | Hum: "); Serial.print(h); Serial.print("%");
  } else {
    Serial.print(" | DHT22: Reading...");
  }
  Serial.print(" | Fan: ");
  Serial.print(fanShouldRun ? "ON" : "OFF");
  Serial.print(" | Score: ");
  Serial.print(safetyScore);
  Serial.print(" | Risk: ");
  Serial.println(riskLevel);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);

  analogSetAttenuation(ADC_11db);

  pinMode(MQ2_DO_PIN, INPUT);
  pinMode(FLAME_AO_PIN, INPUT);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(FAN_PIN, OUTPUT);

  digitalWrite(GREEN_LED_PIN, HIGH);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);
  digitalWrite(FAN_PIN, LOW);

  dht.begin();

  Blynk.config(BLYNK_AUTH_TOKEN, "sgp1.blynk.cloud", 80);

  WiFi.begin(ssid, pass);
  Serial.print("Connecting to Wi-Fi...");
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 20) {
    delay(500);
    Serial.print(".");
    timeout++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi Connected! IP: ");
    Serial.println(WiFi.localIP());
    Blynk.connect(5000); 
  } else {
    Serial.println("\nWi-Fi not found. Running in standalone local mode.");
  }

  sendSensorData();
  timer.setInterval(2000L, sendSensorData);
}

// ==================== MAIN LOOP ====================
void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (Blynk.connected()) {
      Blynk.run();
    } else {
      Blynk.connect(1000); 
    }
  } else {
    static unsigned long lastReconnectAttempt = 0;
    if (millis() - lastReconnectAttempt > 10000) {
      lastReconnectAttempt = millis();
      WiFi.begin(ssid, pass);
    }
  }

  timer.run();
}