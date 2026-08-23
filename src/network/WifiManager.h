#pragma once

#include <stdint.h>

#include <string>

// Sec. 3.4.3 - optional static IP, alternative to DHCP. All string fields
// are borrowed pointers into secrets.h string literals, same lifetime
// assumption already made for ssid/password.
struct StaticIpConfig {
  bool enabled = false;
  const char* ip = "";
  const char* gateway = "";
  const char* subnet = "";
  const char* dns1 = "";
  const char* dns2 = "";  // optional

  StaticIpConfig() = default;
  StaticIpConfig(bool enabled_, const char* ip_, const char* gateway_, const char* subnet_,
                 const char* dns1_, const char* dns2_)
      : enabled(enabled_), ip(ip_), gateway(gateway_), subnet(subnet_), dns1(dns1_), dns2(dns2_) {}
};

// Sec. 3.4.2 - WiFi connection with non-blocking exponential backoff. Not
// testable via the host-side harness (depends on the ESP32 WiFi library).
void initWifi(const char* ssid, const char* password, const StaticIpConfig& staticIp);

// To be called on every loop() cycle.
void tickWifi(uint32_t nowMillis);

bool isWifiConnected();

// Sec. 12.2 - state exposed in /status.
std::string wifiSsid();
int wifiRssi();
uint32_t wifiCurrentBackoffAttempt();
bool isStaticIpActive();
