#pragma once
// Uses unsigned subtraction so an ESP32 millis() rollover is safe.
inline unsigned long trainingRemaining(bool ready, unsigned long completed, unsigned long now) {
  const unsigned long elapsed = (now-completed) & 0xffffffffUL;
  return ready && elapsed<300000UL ? 300000UL-elapsed : 0;
}
