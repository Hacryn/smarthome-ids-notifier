#include "HelpPaginator.h"

std::vector<std::string> paginateBlocks(const std::vector<std::string>& blocks, size_t maxBytes) {
  std::vector<std::string> pages;
  std::string current;

  for (const auto& block : blocks) {
    if (block.size() > maxBytes) {
      if (!current.empty()) {
        pages.push_back(current);
        current.clear();
      }
      pages.push_back(block);  // oversized: alone on its own page, not truncated
      continue;
    }

    size_t candidateSize = current.empty() ? block.size() : current.size() + 2 + block.size();
    if (candidateSize > maxBytes) {
      pages.push_back(current);
      current = block;
    } else {
      if (!current.empty()) current += "\n\n";
      current += block;
    }
  }

  if (!current.empty()) pages.push_back(current);
  return pages;
}
