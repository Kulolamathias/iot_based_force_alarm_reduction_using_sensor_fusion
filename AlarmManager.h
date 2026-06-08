#ifndef ALARM_MANAGER_H
#define ALARM_MANAGER_H

#include <Arduino.h>

class AlarmManager {
  private:
    uint8_t _buzzerPin;
    uint8_t _redLedPin;
    uint8_t _greenLedPin;
    uint8_t _yellowLedPin;
    uint8_t _relayPin;
    int     _buzzerFreq;
    int     _alarmBeepOn;
    int     _alarmBeepOff;
    int     _warningBeepOn;
    int     _warningBeepOff;
    
    enum AlarmState { STATE_SAFE, STATE_WARNING_ONLY, STATE_FULL_ALARM };
    AlarmState _currentState;
    AlarmState _targetState;

    unsigned long _lastBuzzerToggle;
    bool _buzzerOn;
    unsigned long _buzzerSilencedUntil;
    
    unsigned long _lastGasLostTime;
    static const unsigned long SAFE_DELAY_MS = 3000;

    void _updateOutputs();

  public:
    AlarmManager(uint8_t buzzerPin, uint8_t redLedPin, uint8_t greenLedPin, uint8_t yellowLedPin, 
                 uint8_t relayPin, int buzzerFreq, int alarmBeepOn, int alarmBeepOff, 
                 int warningBeepOn, int warningBeepOff);
    void begin();
    
    void triggerWarningOnly();
    void triggerFullAlarm();
    void returnToSafe();
    void forceSafe();
    void silenceBuzzer(unsigned long durationMs);
    void update();
    
    const char* getStateString();
};

#endif