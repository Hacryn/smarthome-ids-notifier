#pragma once

#include <stdint.h>

// Sec. 9.1/9.3 - filtered rewrite of notif_<chat_id>.jsonl. Unlike the
// event log, deletability is decided by the fold (sec. 7.2 - "the last
// row wins" for each id/status pair), not by a single row: the file is by
// design almost always empty or minimal (sec. 7.2), so the cap/repetition
// from sec. 9.3.1 isn't needed. Not testable via the host-side harness.
bool rotateNotificationLog(int64_t chatId, uint32_t cutoff);
