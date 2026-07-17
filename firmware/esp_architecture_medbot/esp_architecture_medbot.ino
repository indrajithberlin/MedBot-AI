#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include "MAX30105.h"
#include "spo2_algorithm.h"
#include <OneWire.h>
#include <DallasTemperature.h>

/* =========================================================================
   NETWORK CONFIGURATION
   ========================================================================= */
const char* ssid = "";         // <--- ENTER WIFI NAME HERE
const char* password = ""; // <--- ENTER WIFI PASS HERE

// IP Address Updated to yours:
const char* serverUrl = ""; 

/* =========================================================================
   HARDWARE PIN DEFINITIONS
   ========================================================================= */
#define I2C_SDA 21
#define I2C_SCL 22
#define ONE_WIRE_BUS 4 

/* =========================================================================
   GLOBAL OBJECTS
   ========================================================================= */
MAX30105 particleSensor;
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature tempSensor(&oneWire);

// Sensor Buffers
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
uint16_t irBuffer[100];
uint16_t redBuffer[100];
#else
uint32_t irBuffer[100]; 
uint32_t redBuffer[100];
#endif

int32_t bufferLength = 100;
int32_t spo2;
int8_t validSPO2;
int32_t heartRate;
int8_t validHeartRate;

// Stabilization settings
const int TARGET_SAMPLES = 30; // 30 valid samples = 100% stable

void setup() {
  Serial.begin(115200);

  // 1. Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.print("Connected! IP: ");
  Serial.println(WiFi.localIP());

  // 2. Initialize Temp Sensor
  tempSensor.begin();
  tempSensor.setWaitForConversion(false);

  // 3. Initialize Heart Rate Sensor
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    Serial.println("MAX30105 not found. Check wiring.");
    while (1);
  }

  // Golden Configuration (Stable readings)
  byte ledBrightness = 60; 
  byte sampleAverage = 4; 
  byte ledMode = 2; 
  int sampleRate = 100; 
  int pulseWidth = 411; 
  int adcRange = 4096; 
  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);

  Serial.println("System Ready. Waiting for finger...");
}

void loop() {
  // Check if finger is on the sensor
  long irValue = particleSensor.getIR();

  if (irValue > 50000) {
    Serial.println("Finger detected! Starting stabilization...");
    performMeasurementAndSend();
  } else {
    // Check less frequently to save power when idle
    delay(100);
  }
}

void performMeasurementAndSend() {
  bufferLength = 100;
  
  // Reset variables for this new measurement
  long samplesTaken = 0;
  long sumHR = 0;
  long sumSpO2 = 0;
  
  // Temperature timing variables
  unsigned long lastTempRequest = 0;
  bool tempRequested = false;
  bool tempReady = false;
  float currentTempC = 0.0;

  // 1. Fill Initial Buffer (4 seconds)
  for (byte i = 0 ; i < bufferLength ; i++) {
    while (particleSensor.available() == false) particleSensor.check();
    redBuffer[i] = particleSensor.getRed();
    irBuffer[i] = particleSensor.getIR();
    particleSensor.nextSample();
  }
  
  // Initial Calculation
  maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

  // Trigger initial temp request
  tempSensor.requestTemperatures();
  lastTempRequest = millis();
  tempRequested = true;

  // 2. Stabilization Loop
  // We stay in here until we get TARGET_SAMPLES valid readings OR finger is removed
  while (samplesTaken < TARGET_SAMPLES) {
    
    // Shift buffer (drop oldest 10, add new 10 for faster updates)
    for (byte i = 10; i < 100; i++) {
      redBuffer[i - 10] = redBuffer[i];
      irBuffer[i - 10] = irBuffer[i];
    }
    
    // Read 10 new samples
    for (byte i = 90; i < 100; i++) {
      while (particleSensor.available() == false) particleSensor.check();
      redBuffer[i] = particleSensor.getRed();
      irBuffer[i] = particleSensor.getIR();
      particleSensor.nextSample();
    }
    
    // Check if finger was lifted
    if (irBuffer[99] < 50000) {
      Serial.println("Finger removed. Resetting...");
      return; // Exit function
    }

    maxim_heart_rate_and_oxygen_saturation(irBuffer, bufferLength, redBuffer, &spo2, &validSPO2, &heartRate, &validHeartRate);

    // --- READ TEMPERATURE (Non-Blocking) ---
    if (tempRequested && (millis() - lastTempRequest >= 750)) {
      currentTempC = tempSensor.getTempCByIndex(0);
      tempRequested = false;
      tempReady = true;
    }

    // --- ACCUMULATE VALID READINGS ---
    // Using your preferred range (40-200 BPM, 80-100% SpO2)
    if (validHeartRate && validSPO2 && heartRate > 40 && heartRate < 200 && spo2 > 80 && spo2 <= 100) {
      sumHR += heartRate;
      sumSpO2 += spo2;
      samplesTaken++;

      // Print Percentage Progress
      Serial.print("Stabilizing... ");
      Serial.print((int)((samplesTaken / (float)TARGET_SAMPLES) * 100));
      Serial.println("%");
    } else {
      Serial.print("."); // Noise/Stabilizing
    }
  }

  // 3. Final Output (Only reached if we hit 100%)
  if (tempReady) {
    int finalHR = sumHR / samplesTaken;
    int finalSpO2 = sumSpO2 / samplesTaken;
    
    Serial.println("100% Stabilized! Sending to Laptop...");
    Serial.print("HR: "); Serial.print(finalHR);
    Serial.print(" SpO2: "); Serial.println(finalSpO2);
    
    sendDataToLaptop(finalHR, finalSpO2, currentTempC);

    // Wait for finger release
    Serial.println("Measurement Sent. Remove finger to reset.");
    while (particleSensor.getIR() > 50000) {
      delay(100);
      particleSensor.check(); // Keep sensor active
    }
    Serial.println("Ready for next person.");
  }
}

void sendDataToLaptop(int hr, int spo2, float temp) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl); 
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{";
    jsonPayload += "\"heart_rate\":" + String(hr) + ",";
    jsonPayload += "\"spo2\":" + String(spo2) + ",";
    jsonPayload += "\"temperature\":" + String(temp);
    jsonPayload += "}";

    int httpResponseCode = http.POST(jsonPayload);

    if (httpResponseCode > 0) {
      Serial.print("Server Response: ");
      Serial.println(httpResponseCode); 
    } else {
      Serial.print("Error sending: ");
      Serial.println(httpResponseCode);
    }
    http.end();
  } else {
    Serial.println("WiFi Disconnected!");
  }
}