#pragma once

#include <stdint.h>

// Sec. 9.4 - filesystem write error counter (after a retry has already
// failed). No dependency on Arduino/LittleFS: the class is pure and
// testable; the shared instance below is the access point used by the
// real write modules.
class FsErrorCounter {
 public:
  // Returns true if this is the first error ever recorded (the caller uses
  // this to decide whether to notify admins, sec. 9.4).
  bool recordFailure();
  uint32_t count() const { return count_; }

 private:
  uint32_t count_ = 0;
};

// Global instance shared by every LittleFS write module, also exposed in
// /status (sec. 12.2).
FsErrorCounter& fsErrorCounter();
