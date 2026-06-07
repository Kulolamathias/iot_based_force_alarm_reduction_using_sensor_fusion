#include "AlarmManager.h"

AlarmManager::AlarmManager(uint8_t buzzerPin, uint8_t redLedPin, uint8_t greenLedPin, uint8_t yellowLedPin, uint8_t relayPin, int buzzerFreq) {
  _buzzerPin = buzzerPin;
  _redLedPin = redLedPin;
  _greenLedPin = greenLedPin;
  _yellowLedPin = yellowLedPin;
  _relayPin = relayPin;
  _buzzerFreq = buzzerFreq;
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
  // Immediately turn off buzzer if it was on
  ledcWriteTone(_buzzerPin, 0);
  _buzzerOn = false;
}

void AlarmManager::update() {
  // State transition
  if (_currentState != _targetState) {
    _currentState = _targetState;
    _updateOutputs();
  }
  
  // Buzzer control (respect silence period)
  bool buzzerMuted = (_buzzerSilencedUntil > millis());
  
  if (!buzzerMuted && _currentState != STATE_SAFE) {
    unsigned long now = millis();
    if ((now - _lastBuzzerToggle) >= 500) {
      _lastBuzzerToggle = now;
      _buzzerOn = !_buzzerOn;
      if (_buzzerOn) {
        ledcWriteTone(_buzzerPin, _buzzerFreq);
      } else {
        ledcWriteTone(_buzzerPin, 0);
      }
    }
  } else {
    // If muted or safe, ensure buzzer off
    ledcWriteTone(_buzzerPin, 0);
    _buzzerOn = false;
  }
}

void AlarmManager::_updateOutputs() {
  // Turn off all LEDs first (common anode/cathode? We use active HIGH)
  digitalWrite(_greenLedPin, LOW);
  digitalWrite(_redLedPin, LOW);
  digitalWrite(_yellowLedPin, LOW);
  
  switch (_currentState) {
    case STATE_SAFE:
      digitalWrite(_greenLedPin, HIGH);
      digitalWrite(_relayPin, HIGH);   // valve open
      break;
      
    case STATE_WARNING_ONLY:
      digitalWrite(_yellowLedPin, HIGH);
      digitalWrite(_relayPin, HIGH);   // valve stays open
      break;
      
    case STATE_FULL_ALARM:
      digitalWrite(_redLedPin, HIGH);
      digitalWrite(_relayPin, LOW);    // valve closed
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