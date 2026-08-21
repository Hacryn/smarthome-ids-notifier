#include "PinMonitor.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "../events/EventTypes.h"

namespace {

constexpr size_t kQueueDepth = 32;  // sez. 3.3 punto 2

QueueHandle_t g_queue = nullptr;
volatile uint32_t g_overflowCount = 0;

struct MonitoredPin {
  uint8_t eventTypeIndex;
  uint8_t gpioPin;
};

MonitoredPin g_monitoredPins[EVENT_TYPES_COUNT];
size_t g_monitoredCount = 0;

// Sez. 3.3 punto 1 - la ISR fa una sola cosa: accodare il record. Nessuna
// allocazione, nessun I/O, nessuna chiamata bloccante.
void IRAM_ATTR isrHandler(void* arg) {
  MonitoredPin* mp = static_cast<MonitoredPin*>(arg);

  PinTransition t;
  t.eventTypeIndex = mp->eventTypeIndex;
  t.level = digitalRead(mp->gpioPin);
  t.millisAtIsr = millis();

  BaseType_t higherPriorityTaskWoken = pdFALSE;
  if (xQueueSendFromISR(g_queue, &t, &higherPriorityTaskWoken) != pdTRUE) {
    g_overflowCount++;
  }
  if (higherPriorityTaskWoken) portYIELD_FROM_ISR();
}

}  // namespace

void initPinMonitor() {
  g_queue = xQueueCreate(kQueueDepth, sizeof(PinTransition));
  g_monitoredCount = 0;

  for (size_t i = 0; i < EVENT_TYPES_COUNT; i++) {
    const EventTypeConfig& cfg = EVENT_TYPES[i];
    if (!cfg.enabled || cfg.pin < 0) continue;

    MonitoredPin& mp = g_monitoredPins[g_monitoredCount++];
    mp.eventTypeIndex = static_cast<uint8_t>(i);
    mp.gpioPin = static_cast<uint8_t>(cfg.pin);

    pinMode(mp.gpioPin, INPUT_PULLUP);
    attachInterruptArg(digitalPinToInterrupt(mp.gpioPin), isrHandler, &mp, CHANGE);
  }
}

size_t drainPinTransitions(void (*onTransition)(const PinTransition&)) {
  PinTransition t;
  size_t count = 0;
  while (xQueueReceive(g_queue, &t, 0) == pdTRUE) {
    onTransition(t);
    count++;
  }
  return count;
}

uint32_t pinQueueOverflowCount() { return g_overflowCount; }
