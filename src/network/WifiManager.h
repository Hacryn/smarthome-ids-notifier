#pragma once

#include <stdint.h>

#include <string>

// Sez. 3.4.2 - connessione WiFi con backoff esponenziale non bloccante.
// Non testabile via harness host-side (dipende dalla libreria WiFi dell'ESP32).
void initWifi(const char* ssid, const char* password);

// Da chiamare ad ogni ciclo di loop().
void tickWifi(uint32_t nowMillis);

bool isWifiConnected();

// Sez. 12.2 - stato esposto in /status.
std::string wifiSsid();
int wifiRssi();
uint32_t wifiCurrentBackoffAttempt();
