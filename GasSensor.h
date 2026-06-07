#ifndef GAS_SENSOR_H
#define GAS_SENSOR_H

#include <Arduino.h>

class GasSensor {
  private:
    uint8_t _mq2Pin;
    uint8_t _mq5Pin;
    int     _mq2Threshold;
    int     _mq5Threshold;
    
    // Calibration constants for PPM calculation (RL = 10k, Ro in clean air)
    float _mq2Ro;   // resistance in clean air (calculated during warm‑up)
    float _mq5Ro;
    bool  _calibrated;
    
    float readResistance(uint8_t pin);   // convert ADC to sensor resistance
    void  autoCalibrate();

  public:
    GasSensor(uint8_t mq2Pin, uint8_t mq5Pin, int mq2Thresh, int mq5Thresh);
    
    void begin();
    int  readMQ2();          // raw ADC
    int  readMQ5();          // raw ADC
    bool isGasDetected();    // raw threshold
    
    float readMQ2_PPM();     // parts per million (LPG/smoke)
    float readMQ5_PPM();     // parts per million (LPG)
    
    void setThresholds(int mq2Thresh, int mq5Thresh);
};

#endif