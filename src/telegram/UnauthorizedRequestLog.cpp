#include "UnauthorizedRequestLog.h"

#include <algorithm>

void recordUnauthorizedRequest(std::vector<UnauthorizedRequest>& buffer,
                                const UnauthorizedRequest& req) {
  auto it = std::find_if(buffer.begin(), buffer.end(),
                          [&](const UnauthorizedRequest& r) { return r.chatId == req.chatId; });
  if (it != buffer.end()) {
    buffer.erase(it);
  } else if (buffer.size() >= kMaxUnauthorizedRequests) {
    buffer.erase(buffer.begin());
  }
  buffer.push_back(req);
}
