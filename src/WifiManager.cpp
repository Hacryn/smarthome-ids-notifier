#include "WifiManager.h"

#include <WiFi.h>

#include "WifiBackoff.h"

namespace {
const char* g_ssid = nullptr;
const char* g_password = nullptr;
uint32_t g_attemptNumber = 0;
uint32_t g_nextAttemptMillis = 0;
}  // namespace

void initWifi(const char* ssid, const char* password) {
  g_ssid = ssid;
  g_password = password;

  WiFi.mode(WIFI_STA);
  g_attemptNumber = 1;
  WiFi.begin(g_ssid, g_password);
  g_nextAttemptMillis = millis() + backoffDelayMs(g_attemptNumber);
}

void tickWifi(uint32_t nowMillis) {
  if (WiFi.status() == WL_CONNECTED) {
    g_attemptNumber = 0;  // sez. 3.4.2 - il contatore si azzera alla riconnessione riuscita
    return;
  }

  if (static_cast<int32_t>(nowMillis - g_nextAttemptMillis) < 0) return;  // in attesa del backoff

  g_attemptNumber++;
  if (shouldForceFullReconnect(g_attemptNumber)) {
    WiFi.disconnect();
    WiFi.begin(g_ssid, g_password);
  } else {
    WiFi.reconnect();
  }
  g_nextAttemptMillis = nowMillis + backoffDelayMs(g_attemptNumber);
}

bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }
