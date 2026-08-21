#pragma once

#include <stdint.h>

constexpr uint32_t kRetryIntervalMs = 60UL * 60UL * 1000UL;  // sec. 6.3 - default 60 minutes

// Sec. 6.3/6.3.1 - non-blocking retry timer with reentrancy protection
// during a recovery scan.
class RetryTimer {
 public:
  // Sec. 6.3 - transient failure: starts the timer if not active, or
  // resets it to the full value if already active (same operation either way).
  void onTransientFailure(uint32_t nowMillis, uint32_t intervalMs);

  // Sec. 6.3 - success in the normal flow (never during a scan, sec.
  // 6.3.1): returns true if an early scan should be triggered. The timer
  // is NOT touched here: the scan's conclusion (endScan) will decide its
  // final state, per the same rule as natural expiry.
  bool onNormalFlowSuccess();

  bool isDue(uint32_t nowMillis) const;

  void beginScan();
  // allResolvedOrAbandoned: outcome of the just-concluded scan.
  void endScan(bool allResolvedOrAbandoned, uint32_t nowMillis, uint32_t intervalMs);

  bool scanInProgress() const { return scanInProgress_; }
  bool isActive() const { return active_; }

 private:
  bool active_ = false;
  uint32_t dueAtMillis_ = 0;
  bool scanInProgress_ = false;
};
