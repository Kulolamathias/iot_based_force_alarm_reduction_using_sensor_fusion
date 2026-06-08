/**
 * @file iot_based_force_alarm_reduction_using_sensor_fusion.ino
 * @brief Main entry point for the gas leak detection system.
 * 
 * Features:
 * - MQ‑2 and MQ‑5 analogue gas sensors (with PPM conversion)
 * - DHT22 temperature & humidity
 * - Buzzer with different patterns for full alarm vs warning
 * - LEDs: red = full alarm, yellow = warning, green = safe
 * - Relay cuts gas valve only on full alarm
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

// ==================== GAS SENSOR THRESHOLDS (RAW ADC) ====================
// Full alarm (dangerous gas) thresholds
#define MQ2_ALARM_RAW       1000   // raw ADC value (0‑4095)
#define MQ5_ALARM_RAW       600

// Warning (possible false leak) thresholds – lower than alarm
#define MQ2_WARNING_RAW     600    // above this triggers warning zone
#define MQ5_WARNING_RAW     400

// ==================== GAS SENSOR THRESHOLDS (PPM) ====================
// Optional: use PPM instead of raw for more accurate detection
#define MQ2_ALARM_PPM       200    // ppm of LPG/smoke to trigger full alarm
#define MQ5_ALARM_PPM       150
#define MQ2_WARNING_PPM     80     // ppm to trigger warning zone
#define MQ5_WARNING_PPM     60

// Choose which method to use for detection (RAW or PPM)
#define USE_PPM_FOR_DETECTION false   // set to true if you prefer PPM

// ==================== ENVIRONMENTAL THRESHOLDS ====================
#define TEMP_WARNING_THRESHOLD      35.0f   // °C – cooking suspicion
#define HUMIDITY_WARNING_THRESHOLD  80.0f   // %RH

// ==================== BUZZER PATTERNS ====================
#define BUZZER_FREQ         2000   // Hz
// Full alarm pattern: fast beep (ms on, ms off)
#define ALARM_BEEP_ON       300
#define ALARM_BEEP_OFF      300
// Warning pattern: polite single short beep, long pause
#define WARNING_BEEP_ON     100
#define WARNING_BEEP_OFF    1900

// ==================== WIFI & MQTT ====================
// #define WIFI_SSID           "Mathias' Sxx U..."
// #define WIFI_PASSWORD       "1234567890223"
#define WIFI_SSID           "pixel 3a"
#define WIFI_PASSWORD       "123@Amoni"
#define MQTT_BROKER         "102.223.8.140"
#define MQTT_PORT           1883
#define MQTT_USERNAME       "mqtt_user"
#define MQTT_PASSWORD       "ega12345"

#define MQTT_TOPIC_STATUS   "gas_alarm/status"
#define MQTT_TOPIC_COMMAND  "gas_alarm/command"

// ==================== GLOBAL OBJECTS ====================
GasSensor       gasSensor(MQ2_PIN, MQ5_PIN, MQ2_ALARM_RAW, MQ5_ALARM_RAW);
DHT22Sensor     dhtSensor(DHTPIN);
AlarmManager    alarmManager(BUZZER_PIN, RED_LED_PIN, GREEN_LED_PIN, YELLOW_LED_PIN, RELAY_PIN, 
                             BUZZER_FREQ, ALARM_BEEP_ON, ALARM_BEEP_OFF, WARNING_BEEP_ON, WARNING_BEEP_OFF);

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
  float mq2PPM = gasSensor.readMQ2_PPM();
  float mq5PPM = gasSensor.readMQ5_PPM();
  
  // 2. Determine gas level using either RAW or PPM (configurable)
  bool isGasAlarm = false;
  bool isGasWarning = false;
  
#if USE_PPM_FOR_DETECTION
  isGasAlarm = (mq2PPM >= MQ2_ALARM_PPM) || (mq5PPM >= MQ5_ALARM_PPM);
  isGasWarning = (!isGasAlarm) && ((mq2PPM >= MQ2_WARNING_PPM) || (mq5PPM >= MQ5_WARNING_PPM));
#else
  isGasAlarm = (mq2Raw >= MQ2_ALARM_RAW) || (mq5Raw >= MQ5_ALARM_RAW);
  isGasWarning = (!isGasAlarm) && ((mq2Raw >= MQ2_WARNING_RAW) || (mq5Raw >= MQ5_WARNING_RAW));
#endif
  
  bool isCookingScenario = (temperature >= TEMP_WARNING_THRESHOLD) ||
                           (humidity >= HUMIDITY_WARNING_THRESHOLD);
  
  // 3. Decision logic
  if (isGasAlarm) {
    alarmManager.triggerFullAlarm();
    Serial.println("-> FULL ALARM – dangerous gas level!");
  }
  else if (isGasWarning) {
    if (isCookingScenario) {
      alarmManager.triggerWarningOnly();
      Serial.println("-> WARNING ONLY (cooking / high temp/humidity)");
    } else {
      // Low gas with normal environment – still warn but politely
      alarmManager.triggerWarningOnly();
      Serial.println("-> LOW GAS WARNING (polite alert)");
    }
  }
  else {
    alarmManager.returnToSafe();
  }
  
  // 4. Update alarm hardware (buzzer, LEDs, relay)
  alarmManager.update();
  
  // 5. Debug output
  Serial.print("Temp: "); Serial.print(temperature); Serial.print(" °C | ");
  Serial.print("Humidity: "); Serial.print(humidity); Serial.print(" % | ");
  Serial.print("MQ2 raw: "); Serial.print(mq2Raw); Serial.print(" (");
  Serial.print(mq2PPM); Serial.print(" ppm) | ");
  Serial.print("MQ5 raw: "); Serial.print(mq5Raw); Serial.print(" (");
  Serial.print(mq5PPM); Serial.print(" ppm) | ");
  Serial.print("Gas: ");
  if (isGasAlarm) Serial.print("ALARM");
  else if (isGasWarning) Serial.print("WARNING");
  else Serial.print("SAFE");
  Serial.println();
  
  // 6. Publish MQTT status
  if (millis() - lastMqttPublish >= MQTT_PUBLISH_INTERVAL) {
    lastMqttPublish = millis();
    const char* stateStr = alarmManager.getStateString();
    bool anyGas = isGasAlarm || isGasWarning;
    publishStatus(temperature, humidity, mq2Raw, mq5Raw, mq2PPM, mq5PPM, anyGas, stateStr);
  }
  
  delay(200);
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

// ==================== MQTT CALLBACK ====================
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  char message[length + 1];
  for (unsigned int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  String msgStr = String(message);
  
  Serial.print("MQTT command received: ");
  Serial.println(msgStr);
  
  if (msgStr.indexOf("\"command\":\"set_thresholds\"") != -1) {
    int newMq2 = msgStr.substring(msgStr.indexOf("\"mq2_alarm\":") + 11).toInt();
    int newMq5 = msgStr.substring(msgStr.indexOf("\"mq5_alarm\":") + 11).toInt();
    gasSensor.setThresholds(newMq2, newMq5);
    Serial.print("Updated alarm thresholds: MQ2=");
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
    lastMqttPublish = 0;
  }
}

// ==================== PUBLISH STATUS VIA MQTT ====================
void publishStatus(float temp, float hum, int mq2Raw, int mq5Raw, float mq2PPM, float mq5PPM, bool gasDetected, const char* alarmState) {
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