#ifndef DHT22_SENSOR_H
#define DHT22_SENSOR_H

#include <Arduino.h>
#include <DHT.h>

/**
 * @class DHT22Sensor
 * @brief Wrapper for DHT22 temperature & humidity sensor.
 */
class DHT22Sensor {
  private:
    DHT _dht;
    bool _lastReadValid;
    float _lastTemp;
    float _lastHum;

  public:
    DHT22Sensor(uint8_t pin);
    void begin();
    float readTemperature();   // Returns °C, or NAN on error
    float readHumidity();      // Returns %RH, or NAN on error
};

#endif