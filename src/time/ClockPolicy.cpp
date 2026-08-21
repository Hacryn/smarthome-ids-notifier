#include "ClockPolicy.h"

bool isEpochPlausible(uint32_t epoch) { return epoch >= kPlausibleEpochThreshold; }
