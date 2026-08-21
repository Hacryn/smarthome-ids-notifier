#pragma once

#include <stdint.h>

#include <string>

// Sec. 3.4.2 - WiFi connection with non-blocking exponential backoff. Not
// testable via the host-side harness (depends on the ESP32 WiFi library).
void initWifi(const char* ssid, const char* password);

// To be called on every loop() cycle.
void tickWifi(uint32_t nowMillis);

bool isWifiConnected();

// Sec. 12.2 - state exposed in /status.
std::string wifiSsid();
int wifiRssi();
uint32_t wifiCurrentBackoffAttempt();
