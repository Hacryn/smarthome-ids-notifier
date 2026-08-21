#pragma once

#include <stdint.h>

#include "../notifications/NotificationPresentation.h"
#include "../notifications/RetryTimer.h"
#include "../rotation/RotationPolicy.h"

// Sec. 11.1 - global configuration, admin-modifiable only. The default
// values mirror the constants already defined in their respective modules
// (sec. 6.4/6.5/6.7/9.1); this struct is the only state the /setXxx
// commands actually modify - the constants remain only the defaults.
struct GlobalConfig {
  uint32_t retentionWeeks = kDefaultRetentionWeeks;
  uint32_t gracePeriodSec = kGracePeriodSec;
  uint32_t retryIntervalMinutes = kRetryIntervalMs / 60000UL;
  uint32_t maxRetries = kMaxRetries;
  uint32_t networkIssueThresholdSec = 120;  // sec. 3.4.1
  uint32_t aggregateThreshold = kAggregateThreshold;
};
