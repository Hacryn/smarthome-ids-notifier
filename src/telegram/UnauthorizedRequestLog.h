#pragma once

#include <stdint.h>

#include <string>
#include <vector>

// Sec. 12.4 - in-RAM-only buffer of the most recent unauthorized command
// attempts (never persisted to LittleFS/NVS, reset on reboot). Backs
// /requests. No time window: capped at kMaxUnauthorizedRequests entries,
// deduped by chatId so one insistent chat_id can't crowd out the others.
inline constexpr size_t kMaxUnauthorizedRequests = 5;

struct UnauthorizedRequest {
  int64_t chatId;
  std::string userId;    // Telegram user id (from UserRead::id())
  std::string username;  // may be empty, Telegram doesn't guarantee one
  uint32_t ts;            // epoch of the request
};

// Records a request. If chatId is already in the buffer, its entry is
// removed and the new one (updated timestamp/username) is appended at the
// end (becoming the most recent) instead of creating a duplicate. If
// chatId is new and the buffer is already at kMaxUnauthorizedRequests, the
// oldest entry (index 0) is discarded first.
void recordUnauthorizedRequest(std::vector<UnauthorizedRequest>& buffer,
                                const UnauthorizedRequest& req);
