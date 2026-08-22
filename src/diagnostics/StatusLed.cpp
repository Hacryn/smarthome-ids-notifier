#include "StatusLed.h"

#include <Arduino.h>

namespace {

constexpr uint32_t kBlinkIntervalMs = 500;

// Sec. 13 - the Nano ESP32's built-in RGB LED is active-low (LOW = on).
void setColor(bool red, bool green, bool blue) {
  digitalWrite(LED_RED, red ? LOW : HIGH);
  digitalWrite(LED_GREEN, green ? LOW : HIGH);
  digitalWrite(LED_BLUE, blue ? LOW : HIGH);
}

}  // namespace

void initStatusLed() {
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  setColor(false, false, false);
}

void tickStatusLed(uint32_t nowMillis, StatusLedState state) {
  switch (state) {
    case StatusLedState::OK:
      setColor(false, true, false);  // green
      break;
    case StatusLedState::NETWORK_OR_TIME:
      setColor(true, true, false);  // yellow (red+green)
      break;
    case StatusLedState::DEGRADED:
      setColor(true, false, true);  // purple (red+blue)
      break;
    case StatusLedState::ALARM: {
      bool on = (nowMillis / kBlinkIntervalMs) % 2 == 0;
      setColor(on, false, false);  // blinking red
      break;
    }
    case StatusLedState::ALARM_AND_NETWORK_OR_TIME: {
      bool showRed = (nowMillis / kBlinkIntervalMs) % 2 == 0;
      setColor(true, !showRed, false);  // alternating red / yellow
      break;
    }
  }
}
