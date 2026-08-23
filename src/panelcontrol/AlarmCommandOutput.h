#pragma once

#include <stdint.h>

#include "AlarmCommandTypes.h"

// Sec. 3.4.3 - drives the 6 output pins that issue remote arm/disarm
// commands to the panel. Not testable via the host-side harness (depends
// on real ESP32 GPIO); the pulse timing itself is covered by
// AlarmPulseTimer's host-side test.

// Sets all 6 pins to OUTPUT, driven LOW (idle = holding GND, matching the
// panel's expected rest state). Call once from setup(), alongside
// initPinMonitor()/initStatusLed().
void initAlarmCommandOutputs();

// Starts a pulse on cfg's pin. Returns false without doing anything if a
// pulse is already in progress (system-wide - only one command in flight
// at a time).
bool triggerAlarmCommand(const AlarmCommandConfig& cfg);

// Call every loop() cycle: releases the pin back to LOW once the pulse
// duration has elapsed.
void tickAlarmCommandOutput(uint32_t nowMillis);

bool isAlarmCommandPulseInProgress();
