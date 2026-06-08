#include "AlarmManager.h"

AlarmManager::AlarmManager(uint8_t buzzerPin, uint8_t redLedPin, uint8_t greenLedPin, uint8_t yellowLedPin, 
                           uint8_t relayPin, int buzzerFreq, int alarmBeepOn, int alarmBeepOff,
                           int warningBeepOn, int warningBeepOff) {
  _buzzerPin = buzzerPin;
  _redLedPin = redLedPin;
  _greenLedPin = greenLedPin;
  _yellowLedPin = yellowLedPin;
  _relayPin = relayPin;
  _buzzerFreq = buzzerFreq;
  _alarmBeepOn = alarmBeepOn;
  _alarmBeepOff = alarmBeepOff;
  _warningBeepOn = warningBeepOn;
  _warningBeepOff = warningBeepOff;
  _currentState = STATE_SAFE;
  _targetState = STATE_SAFE;
  _lastGasLostTime = 0;
  _lastBuzzerToggle = 0;
  _buzzerOn = false;
  _buzzerSilencedUntil = 0;
}

void AlarmManager::begin() {
  pinMode(_redLedPin, OUTPUT);
  pinMode(_greenLedPin, OUTPUT);
  pinMode(_yellowLedPin, OUTPUT);
  pinMode(_relayPin, OUTPUT);
  
  ledcAttach(_buzzerPin, _buzzerFreq, 8);
  ledcWriteTone(_buzzerPin, 0);
  
  _currentState = STATE_SAFE;
  _targetState = STATE_SAFE;
  _updateOutputs();
}

void AlarmManager::triggerWarningOnly() {
  _targetState = STATE_WARNING_ONLY;
  _lastGasLostTime = 0;
}

void AlarmManager::triggerFullAlarm() {
  _targetState = STATE_FULL_ALARM;
  _lastGasLostTime = 0;
}

void AlarmManager::returnToSafe() {
  if (_targetState != STATE_SAFE && _lastGasLostTime == 0) {
    _lastGasLostTime = millis();
  }
  if (_lastGasLostTime != 0 && (millis() - _lastGasLostTime) >= SAFE_DELAY_MS) {
    _targetState = STATE_SAFE;
    _lastGasLostTime = 0;
  }
}

void AlarmManager::forceSafe() {
  _targetState = STATE_SAFE;
  _lastGasLostTime = 0;
}

void AlarmManager::silenceBuzzer(unsigned long durationMs) {
  _buzzerSilencedUntil = millis() + durationMs;
  ledcWriteTone(_buzzerPin, 0);
  _buzzerOn = false;
}

void AlarmManager::update() {
  if (_currentState != _targetState) {
    _currentState = _targetState;
    _updateOutputs();
  }
  
  bool buzzerMuted = (_buzzerSilencedUntil > millis());
  
  if (!buzzerMuted && _currentState != STATE_SAFE) {
    unsigned long now = millis();
    int beepOn = (_currentState == STATE_FULL_ALARM) ? _alarmBeepOn : _warningBeepOn;
    int beepOff = (_currentState == STATE_FULL_ALARM) ? _alarmBeepOff : _warningBeepOff;
    int cycleDuration = beepOn + beepOff;
    
    if (_buzzerOn) {
      // Currently beeping – check if it's time to turn off
      if (now - _lastBuzzerToggle >= beepOn) {
        ledcWriteTone(_buzzerPin, 0);
        _buzzerOn = false;
        _lastBuzzerToggle = now;
      }
    } else {
      // Currently silent – check if it's time to beep
      if (now - _lastBuzzerToggle >= beepOff) {
        ledcWriteTone(_buzzerPin, _buzzerFreq);
        _buzzerOn = true;
        _lastBuzzerToggle = now;
      }
    }
  } else {
    ledcWriteTone(_buzzerPin, 0);
    _buzzerOn = false;
  }
}

void AlarmManager::_updateOutputs() {
  digitalWrite(_greenLedPin, LOW);
  digitalWrite(_redLedPin, LOW);
  digitalWrite(_yellowLedPin, LOW);
  
  switch (_currentState) {
    case STATE_SAFE:
      digitalWrite(_greenLedPin, HIGH);
      digitalWrite(_relayPin, HIGH);
      break;
      
    case STATE_WARNING_ONLY:
      digitalWrite(_yellowLedPin, HIGH);
      digitalWrite(_relayPin, HIGH);
      break;
      
    case STATE_FULL_ALARM:
      digitalWrite(_redLedPin, HIGH);
      digitalWrite(_relayPin, LOW);
      break;
  }
}

const char* AlarmManager::getStateString() {
  switch (_currentState) {
    case STATE_SAFE:        return "safe";
    case STATE_WARNING_ONLY: return "warning_only";
    case STATE_FULL_ALARM:  return "full_alarm";
    default:                return "unknown";
  }
}