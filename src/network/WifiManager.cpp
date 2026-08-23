#include "WifiManager.h"

#include <WiFi.h>

#include "../diagnostics/SerialLog.h"
#include "WifiBackoff.h"

namespace {
const char* g_ssid = nullptr;
const char* g_password = nullptr;
StaticIpConfig g_staticIp;
bool g_staticIpActive = false;
uint32_t g_attemptNumber = 0;
uint32_t g_nextAttemptMillis = 0;

// Sec. 3.4.3 - returns true only if every required field is present and
// parses as a valid dotted-decimal address. Applies WiFi.config()
// immediately on success; on any failure, logs why and leaves WiFi on
// DHCP (no partial application - WiFi.config() is only called if every
// required field already validated).
bool applyStaticIpIfConfigured(const StaticIpConfig& cfg) {
  if (!cfg.enabled) return false;

  IPAddress ip, gateway, subnet, dns1, dns2;
  if (!ip.fromString(cfg.ip)) {
    logWarn("Static IP enabled but STATIC_IP_ADDRESS is missing/invalid, falling back to DHCP");
    return false;
  }
  if (!gateway.fromString(cfg.gateway)) {
    logWarn("Static IP enabled but STATIC_IP_GATEWAY is missing/invalid, falling back to DHCP");
    return false;
  }
  if (!subnet.fromString(cfg.subnet)) {
    logWarn("Static IP enabled but STATIC_IP_SUBNET is missing/invalid, falling back to DHCP");
    return false;
  }
  if (!dns1.fromString(cfg.dns1)) {
    logWarn("Static IP enabled but STATIC_IP_DNS1 is missing/invalid, falling back to DHCP");
    return false;
  }
  if (cfg.dns2 && cfg.dns2[0] != '\0' && !dns2.fromString(cfg.dns2)) {
    logWarn("STATIC_IP_DNS2 is set but not a valid address, ignoring it (DNS1 still applies)");
    dns2 = IPAddress();  // 0.0.0.0 - WiFi.config() treats this as "not set"
  }

  WiFi.config(ip, gateway, subnet, dns1, dns2);
  logInfo("Static IP configured: %s", cfg.ip);
  return true;
}
}  // namespace

void initWifi(const char* ssid, const char* password, const StaticIpConfig& staticIp) {
  g_ssid = ssid;
  g_password = password;
  g_staticIp = staticIp;

  WiFi.mode(WIFI_STA);
  g_staticIpActive = applyStaticIpIfConfigured(g_staticIp);
  g_attemptNumber = 1;
  WiFi.begin(g_ssid, g_password);
  g_nextAttemptMillis = millis() + backoffDelayMs(g_attemptNumber);
}

void tickWifi(uint32_t nowMillis) {
  if (WiFi.status() == WL_CONNECTED) {
    if (g_attemptNumber != 0) logInfo("WiFi connected (SSID=%s)", WiFi.SSID().c_str());
    g_attemptNumber = 0;  // sec. 3.4.2 - the counter resets on a successful reconnection
    return;
  }

  if (static_cast<int32_t>(nowMillis - g_nextAttemptMillis) < 0) return;  // waiting on backoff

  g_attemptNumber++;
  logWarn("WiFi disconnected, backoff attempt #%lu", static_cast<unsigned long>(g_attemptNumber));
  if (shouldForceFullReconnect(g_attemptNumber)) {
    WiFi.disconnect();
    g_staticIpActive = applyStaticIpIfConfigured(g_staticIp);  // re-assert static config after disconnect()
    WiFi.begin(g_ssid, g_password);
  } else {
    WiFi.reconnect();
  }
  g_nextAttemptMillis = nowMillis + backoffDelayMs(g_attemptNumber);
}

bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }

std::string wifiSsid() { return std::string(WiFi.SSID().c_str()); }

int wifiRssi() { return WiFi.RSSI(); }

uint32_t wifiCurrentBackoffAttempt() { return g_attemptNumber; }

bool isStaticIpActive() { return g_staticIpActive; }
