#include "FsErrorCounter.h"

bool FsErrorCounter::recordFailure() {
  count_++;
  return count_ == 1;
}

FsErrorCounter& fsErrorCounter() {
  static FsErrorCounter instance;
  return instance;
}
