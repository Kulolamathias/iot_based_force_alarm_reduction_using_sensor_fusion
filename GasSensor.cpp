#include "GasSensor.h"

// Known constants from datasheets (for LPG detection)
// MQ2: Rs/Ro = a * ppm^b  =>  ppm = (Rs/Ro / a)^(1/b)
// Typical a=1000, b=-0.45 for LPG
// MQ5: a=1000, b=-0.45 for LPG
// We'll use simplified formulas after calibration.

GasSensor::GasSensor(uint8_t mq2Pin, uint8_t mq5Pin, int mq2Thresh, int mq5Thresh) {
  _mq2Pin = mq2Pin;
  _mq5Pin = mq5Pin;
  _mq2Threshold = mq2Thresh;
  _mq5Threshold = mq5Thresh;
  _mq2Ro = 1.0;
  _mq5Ro = 1.0;
  _calibrated = false;
}

void GasSensor::begin() {
  pinMode(_mq2Pin, INPUT);
  pinMode(_mq5Pin, INPUT);
  analogReadResolution(12);
  
  // Auto‑calibrate Ro in clean air (run for 30 seconds)
  Serial.println("Calibrating MQ sensors in clean air...");
  autoCalibrate();
  _calibrated = true;
}

void GasSensor::autoCalibrate() {
  const int samples = 100;
  float sumMQ2 = 0, sumMQ5 = 0;
  for (int i = 0; i < samples; i++) {
    sumMQ2 += analogRead(_mq2Pin);
    sumMQ5 += analogRead(_mq5Pin);
    delay(50);
  }
  float avgMQ2 = sumMQ2 / samples;
  float avgMQ5 = sumMQ5 / samples;
  
  // For MQ2 and MQ5 with 10k load resistor, Ro = (Vc / Vout - 1) * RL
  // Assuming Vc = 3.3V (ESP ADC reference), RL = 10kΩ
  float vOutMQ2 = (avgMQ2 / 4095.0) * 3.3;
  float vOutMQ5 = (avgMQ5 / 4095.0) * 3.3;
  _mq2Ro = (3.3 / vOutMQ2 - 1.0) * 10.0;  // in kΩ
  _mq5Ro = (3.3 / vOutMQ5 - 1.0) * 10.0;
  
  Serial.print("MQ2 Ro = "); Serial.print(_mq2Ro); Serial.println(" kΩ");
  Serial.print("MQ5 Ro = "); Serial.print(_mq5Ro); Serial.println(" kΩ");
}

float GasSensor::readResistance(uint8_t pin) {
  int adc = analogRead(pin);
  float voltage = (adc / 4095.0) * 3.3;
  if (voltage == 0) return 100.0; // prevent division by zero
  float resistance = (3.3 / voltage - 1.0) * 10.0; // RL=10kΩ
  return resistance;
}

int GasSensor::readMQ2() {
  return analogRead(_mq2Pin);
}

int GasSensor::readMQ5() {
  return analogRead(_mq5Pin);
}

bool GasSensor::isGasDetected() {
  return (readMQ2() >= _mq2Threshold) || (readMQ5() >= _mq5Threshold);
}

float GasSensor::readMQ2_PPM() {
  if (!_calibrated) return 0;
  float rs = readResistance(_mq2Pin);
  float ratio = rs / _mq2Ro;
  // Formula for LPG: ppm = 1000 * (ratio)^(-2.95) (typical for MQ2)
  // But we use a more common approximation: ppm = 10^((log10(ratio) + 1.5) / -0.45)
  // Simpler: ppm = 1000 * pow(ratio, -0.45);
  float ppm = 1000.0 * pow(ratio, -0.45);
  if (ppm < 0) ppm = 0;
  return ppm;
}

float GasSensor::readMQ5_PPM() {
  if (!_calibrated) return 0;
  float rs = readResistance(_mq5Pin);
  float ratio = rs / _mq5Ro;
  // MQ5 for LPG: ppm = 1000 * pow(ratio, -0.45)
  float ppm = 1000.0 * pow(ratio, -0.45);
  if (ppm < 0) ppm = 0;
  return ppm;
}

void GasSensor::setThresholds(int mq2Thresh, int mq5Thresh) {
  _mq2Threshold = mq2Thresh;
  _mq5Threshold = mq5Thresh;
}