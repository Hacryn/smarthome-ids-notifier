#pragma once

#include <stdint.h>

// Sec. 9.3 - filtered rewrite of log.jsonl (LittleFS doesn't support
// selective row deletion). Not testable via the host-side harness (depends
// on real LittleFS); the id-collection algorithm (DeletableIdCollector) is
// tested separately.

// Returns the number of pass1+pass2+commit cycles executed (0 if there was
// nothing to delete), or -1 if a write/rename failed.
int rotateEventLog(uint32_t cutoff);

// Sec. 9.3.2 - removes any leftover temp file from a rotation interrupted
// by a blackout. Call in setup().
void cleanupStaleEventLogRotation();
