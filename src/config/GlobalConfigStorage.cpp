#include "GlobalConfigStorage.h"

#include <Preferences.h>

namespace {
const char* kNamespace = "notifier";
}  // namespace

GlobalConfig loadGlobalConfig() {
  GlobalConfig cfg;  // default values if the keys don't exist yet

  Preferences prefs;
  prefs.begin(kNamespace, true);
  cfg.retentionWeeks = prefs.getULong("cfg_retention_w", cfg.retentionWeeks);
  cfg.gracePeriodSec = prefs.getULong("cfg_grace_s", cfg.gracePeriodSec);
  cfg.retryIntervalMinutes = prefs.getULong("cfg_retry_min", cfg.retryIntervalMinutes);
  cfg.maxRetries = prefs.getULong("cfg_max_retry", cfg.maxRetries);
  cfg.networkIssueThresholdSec = prefs.getULong("cfg_net_thr_s", cfg.networkIssueThresholdSec);
  cfg.aggregateThreshold = prefs.getULong("cfg_agg_thr", cfg.aggregateThreshold);
  prefs.end();

  return cfg;
}

namespace {
GlobalConfig g_store;
}  // namespace

GlobalConfig& globalConfig() { return g_store; }

void initGlobalConfigStore() { g_store = loadGlobalConfig(); }

void saveGlobalConfig(const GlobalConfig& cfg) {
  Preferences prefs;
  prefs.begin(kNamespace, false);
  prefs.putULong("cfg_retention_w", cfg.retentionWeeks);
  prefs.putULong("cfg_grace_s", cfg.gracePeriodSec);
  prefs.putULong("cfg_retry_min", cfg.retryIntervalMinutes);
  prefs.putULong("cfg_max_retry", cfg.maxRetries);
  prefs.putULong("cfg_net_thr_s", cfg.networkIssueThresholdSec);
  prefs.putULong("cfg_agg_thr", cfg.aggregateThreshold);
  prefs.end();
}
