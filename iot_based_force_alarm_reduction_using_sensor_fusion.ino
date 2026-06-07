/**
 * @file project.ino
 * @brief Main entry point for the gas leak detection system.
 * 
 * Features:
 * - MQ‑2 and MQ‑5 analogue gas sensors (with PPM conversion)
 * - DHT22 temperature & humidity
 * - Buzzer, RGB‑like LEDs (red = full alarm, yellow = warning, green = safe)
 * - Relay to cut gas valve on full alarm
 * - WiFi + MQTT for remote monitoring and control
 * - Intelligent false‑alarm reduction (cooking scenario detection)
 */

#include <WiFi.h>
#include <PubSubClient.h>
#include "GasSensor.h"
#include "DHT22Sensor.h"
#include "AlarmManager.h"

// ==================== PIN DEFINITIONS ====================
#define MQ2_PIN           6    // ADC1, safe
#define MQ5_PIN           7    // ADC1, safe
#define DHTPIN            4    // digital, safe
#define BUZZER_PIN       16    // LEDC capable
#define RED_LED_PIN      17    // full alarm indicator
#define GREEN_LED_PIN    18    // safe indicator
#define YELLOW_LED_PIN   15    // warning (false leak) indicator
#define RELAY_PIN        10    // 

// ==================== SENSOR THRESHOLDS ====================
#define MQ2_THRESHOLD_RAW    1000   // raw ADC value (0‑4095) – gas present if above
#define MQ5_THRESHOLD_RAW    600

// PPM thresholds for alarming (more accurate than raw ADC)
#define MQ2_PPM_THRESHOLD    200   // ppm of LPG/smoke to trigger alarm
#define MQ5_PPM_THRESHOLD    150   // ppm of LPG

// ==================== ENVIRONMENTAL THRESHOLDS ====================
#define TEMP_WARNING_THRESHOLD      35.0f   // °C – cooking suspicion
#define HUMIDITY_WARNING_THRESHOLD  80.0f   // %RH

// ==================== BUZZER ====================
#define BUZZER_FREQ         2000   // Hz

// ==================== WIFI & MQTT ====================
#define WIFI_SSID           "Mathias' Sxx U..."
#define WIFI_PASSWORD       "1234567890223"
#define MQTT_BROKER         "102.223.8.140"
#define MQTT_PORT           1883
#define MQTT_USERNAME       "mqtt_user"
#define MQTT_PASSWORD       "ega12345"

#define MQTT_TOPIC_STATUS   "gas_alarm/status"
#define MQTT_TOPIC_COMMAND  "gas_alarm/command"

// ==================== GLOBAL OBJECTS ====================
GasSensor       gasSensor(MQ2_PIN, MQ5_PIN, MQ2_THRESHOLD_RAW, MQ5_THRESHOLD_RAW);
DHT22Sensor     dhtSensor(DHTPIN);
AlarmManager    alarmManager(BUZZER_PIN, RED_LED_PIN, GREEN_LED_PIN, YELLOW_LED_PIN, RELAY_PIN, BUZZER_FREQ);

WiFiClient      wifiClient;
PubSubClient    mqttClient(wifiClient);

unsigned long   lastMqttPublish = 0;
const unsigned long MQTT_PUBLISH_INTERVAL = 5000; // 5 seconds

// ==================== FUNCTION PROTOTYPES ====================
void connectWiFi();
void connectMQTT();
void mqttCallback(char* topic, byte* payload, unsigned int length);
void publishStatus(float temp, float hum, int mq2Raw, int mq5Raw, float mq2PPM, float mq5PPM, bool gasDetected, const char* alarmState);

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  
  // Initialise sensors and alarm manager
  gasSensor.begin();
  dhtSensor.begin();
  alarmManager.begin();
  
  // Connect to WiFi and MQTT
  connectWiFi();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);
  connectMQTT();
  
  // Warm‑up time for MQ sensors (10‑30 seconds)
  Serial.println("Warming up gas sensors...");
  delay(10000);
  Serial.println("System ready.");
}

// ==================== MAIN LOOP ====================
void loop() {
  // Keep MQTT connection alive
  if (!mqttClient.connected()) {
    connectMQTT();
  }
  mqttClient.loop();
  
  // 1. Read all sensors
  float temperature = dhtSensor.readTemperature();
  float humidity    = dhtSensor.readHumidity();
  
  int mq2Raw = gasSensor.readMQ2();
  int mq5Raw = gasSensor.readMQ5();
  
  // Convert raw ADC to PPM (using sensor‑specific formulas)
  float mq2PPM = gasSensor.readMQ2_PPM();
  float mq5PPM = gasSensor.readMQ5_PPM();
  
  // Gas detection logic (using raw OR ppm thresholds – you can choose)
  bool gasDetected = gasSensor.isGasDetected();           // raw threshold
  // Alternatively use PPM: (mq2PPM >= MQ2_PPM_THRESHOLD || mq5PPM >= MQ5_PPM_THRESHOLD);
  
  // 2. Debug output
  Serial.print("Temp: "); Serial.print(temperature); Serial.print(" °C | ");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.print(" % | ");
  Serial.print("MQ2 raw: "); Serial.print(mq2Raw); Serial.print(" (");
  Serial.print(mq2PPM); Serial.print(" ppm) | ");
  Serial.print("MQ5 raw: "); Serial.print(mq5Raw); Serial.print(" (");
  Serial.print(mq5PPM); Serial.print(" ppm) | ");
  Serial.print("Gas detected: "); Serial.println(gasDetected ? "YES" : "NO");
  
  // 3. Intelligent decision logic
  if (gasDetected) {
    bool isCookingScenario = (temperature >= TEMP_WARNING_THRESHOLD) ||
                             (humidity >= HUMIDITY_WARNING_THRESHOLD);
    
    if (isCookingScenario) {
      alarmManager.triggerWarningOnly();   // false leak: yellow LED, buzzer, valve open
      Serial.println("-> WARNING ONLY (cooking / high temp/humidity)");
    } else {
      alarmManager.triggerFullAlarm();     // true leak: red LED, buzzer, valve closed
      Serial.println("-> FULL ALARM – gas valve closed!");
    }
  } else {
    alarmManager.returnToSafe();           // no gas -> green LED, valve open
  }
  
  // 4. Update alarm hardware (buzzer, LEDs, relay)
  alarmManager.update();
  
  // 5. Publish MQTT status periodically
  if (millis() - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMqttPublish = millis();
    const char* stateStr = alarmManager.getStateString(); // you need to add this method
    publishStatus(temperature, humidity, mq2Raw, mq5Raw, mq2PPM, mq5PPM, gasDetected, stateStr);
  }
  
  delay(200);  // smaller delay for responsive MQTT
}

// ==================== WIFI CONNECTION ====================
void connectWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected, IP: " + WiFi.localIP().toString());
}

// ==================== MQTT CONNECTION ====================
void connectMQTT() {
  while (!mqttClient.connected()) {
    Serial.print("Connecting to MQTT...");
    if (mqttClient.connect("ESP32S3_GasDetector", MQTT_USERNAME, MQTT_PASSWORD)) {
      Serial.println("connected");
      mqttClient.subscribe(MQTT_TOPIC_COMMAND);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" retry in 5 sec");
      delay(5000);
    }
  }
}

// ==================== MQTT CALLBACK (receive commands) ====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  // Convert payload to string
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  String msgStr = String(message);
  
  Serial.print("MQTT command received: ");
  Serial.println(msgStr);
  
  // Simple JSON parsing (you can use ArduinoJson library for complex commands)
  if (msgStr.indexOf("\"command\":\"set_thresholds\"") != -1) {
    // Extract values – this is basic; for production use ArduinoJson
    int newMq2 = msgStr.substring(msgStr.indexOf("\"mq2\":") + 6).toInt();
    int newMq5 = msgStr.substring(msgStr.indexOf("\"mq5\":") + 6).toInt();
    gasSensor.setThresholds(newMq2, newMq5);
    Serial.print("Updated thresholds: MQ2=");
    Serial.print(newMq2);
    Serial.print(", MQ5=");
    Serial.println(newMq5);
  }
  else if (msgStr.indexOf("\"command\":\"force_state\"") != -1) {
    if (msgStr.indexOf("\"state\":\"safe\"") != -1) {
      alarmManager.forceSafe();
    } else if (msgStr.indexOf("\"state\":\"warning_only\"") != -1) {
      alarmManager.triggerWarningOnly();
    } else if (msgStr.indexOf("\"state\":\"full_alarm\"") != -1) {
      alarmManager.triggerFullAlarm();
    }
  }
  else if (msgStr.indexOf("\"command\":\"silence_buzzer\"") != -1) {
    int duration = msgStr.substring(msgStr.indexOf("\"duration_seconds\":") + 19).toInt();
    alarmManager.silenceBuzzer(duration * 1000UL);
  }
  else if (msgStr.indexOf("\"command\":\"get_status\"") != -1) {
    // Force a publish immediately (will happen on next loop iteration)
    lastMqttPublish = 0;
  }
}

// ==================== PUBLISH STATUS VIA MQTT ====================
void publishStatus(float temp, float hum, int mq2Raw, int mq5Raw, float mq2PPM, float mq5PPM, bool gasDetected, const char* alarmState) {
  // Build JSON string (manually – for simplicity)
  String json = "{";
  json += "\"timestamp_ms\":" + String(millis()) + ",";
  json += "\"temperature_c\":" + String(temp, 1) + ",";
  json += "\"humidity_pct\":" + String(hum, 1) + ",";
  json += "\"mq2_raw\":" + String(mq2Raw) + ",";
  json += "\"mq5_raw\":" + String(mq5Raw) + ",";
  json += "\"mq2_ppm\":" + String(mq2PPM, 0) + ",";
  json += "\"mq5_ppm\":" + String(mq5PPM, 0) + ",";
  json += "\"gas_detected\":" + String(gasDetected ? "true" : "false") + ",";
  json += "\"alarm_state\":\"" + String(alarmState) + "\"";
  json += "}";
  
  if (mqttClient.publish(MQTT_TOPIC_STATUS, json.c_str())) {
    Serial.println("MQTT status published");
  } else {
    Serial.println("MQTT publish failed");
  }
}