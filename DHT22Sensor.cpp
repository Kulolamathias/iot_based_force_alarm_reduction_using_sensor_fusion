#include "DHT22Sensor.h"

DHT22Sensor::DHT22Sensor(uint8_t pin) : _dht(pin, DHT22) {
  _lastReadValid = false;
  _lastTemp = NAN;
  _lastHum = NAN;
}

void DHT22Sensor::begin() {
  _dht.begin();
}

float DHT22Sensor::readTemperature() {
  float t = _dht.readTemperature();
  if (isnan(t)) {
    // Return last valid value if available, otherwise NAN
    return _lastReadValid ? _lastTemp : NAN;
  }
  _lastTemp = t;
  _lastHum = _dht.readHumidity();  // also update humidity cache
  _lastReadValid = true;
  return t;
}

float DHT22Sensor::readHumidity() {
  float h = _dht.readHumidity();
  if (isnan(h)) {
    return _lastReadValid ? _lastHum : NAN;
  }
  _lastHum = h;
  _lastReadValid = true;
  return h;
}