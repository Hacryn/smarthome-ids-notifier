#pragma once

#include <stdint.h>

// Sez. 3.4.2 - connessione WiFi con backoff esponenziale non bloccante.
// Non testabile via harness host-side (dipende dalla libreria WiFi dell'ESP32).
void initWifi(const char* ssid, const char* password);

// Da chiamare ad ogni ciclo di loop().
void tickWifi(uint32_t nowMillis);

bool isWifiConnected();
