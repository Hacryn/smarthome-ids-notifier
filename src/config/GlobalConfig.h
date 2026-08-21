#pragma once

#include <stdint.h>

#include "../notifications/NotificationPresentation.h"
#include "../notifications/RetryTimer.h"
#include "../rotation/RotationPolicy.h"

// Sez. 11.1 - configurazioni globali, modificabili solo da admin. I valori
// di default richiamano le costanti gia' definite nei rispettivi moduli
// (sez. 6.4/6.5/6.7/9.1); questo struct e' l'unico stato che i comandi
// /setXxx (fase 12) modificano davvero, le costanti restano solo i default.
struct GlobalConfig {
  uint32_t retentionWeeks = kDefaultRetentionWeeks;
  uint32_t gracePeriodSec = kGracePeriodSec;
  uint32_t retryIntervalMinutes = kRetryIntervalMs / 60000UL;
  uint32_t maxRetries = kMaxRetries;
  uint32_t networkIssueThresholdSec = 120;  // sez. 3.4.1
  uint32_t aggregateThreshold = kAggregateThreshold;
};
