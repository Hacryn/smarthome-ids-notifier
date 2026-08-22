#pragma once

#include <string>
#include <vector>

// Sec. 14 - packs command blocks (already-formatted HTML, one per command)
// into pages no larger than maxBytes each, joined by a blank line. A single
// block larger than maxBytes still ends up alone on its own page (no silent
// truncation). Pure and generic: no manual rebalancing needed when a future
// command is added to /help.
std::vector<std::string> paginateBlocks(const std::vector<std::string>& blocks, size_t maxBytes);
