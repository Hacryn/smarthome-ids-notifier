#include <cassert>
#include <cstdio>

#include "../src/telegram/HelpPaginator.h"

static void test_small_blocks_fit_in_one_page() {
  std::vector<std::string> blocks = {"a", "b", "c"};
  std::vector<std::string> pages = paginateBlocks(blocks, 100);

  assert(pages.size() == 1);
  assert(pages[0] == "a\n\nb\n\nc");
}

static void test_blocks_split_across_multiple_pages() {
  // Each block is 10 bytes; a budget of 12 fits only one block per page
  // (block + would-be separator pushes any second block over budget).
  std::vector<std::string> blocks = {"0123456789", "abcdefghij", "ABCDEFGHIJ"};
  std::vector<std::string> pages = paginateBlocks(blocks, 12);

  assert(pages.size() == 3);
  assert(pages[0] == "0123456789");
  assert(pages[1] == "abcdefghij");
  assert(pages[2] == "ABCDEFGHIJ");
}

static void test_oversized_single_block_alone_on_its_own_page_not_truncated() {
  std::string huge(50, 'x');
  std::vector<std::string> blocks = {"small", huge, "also small"};
  std::vector<std::string> pages = paginateBlocks(blocks, 10);

  assert(pages.size() == 3);
  assert(pages[0] == "small");
  assert(pages[1] == huge);
  assert(pages[1].size() == 50);  // not truncated despite exceeding maxBytes
  assert(pages[2] == "also small");
}

static void test_empty_input_yields_no_pages() {
  std::vector<std::string> pages = paginateBlocks({}, 100);
  assert(pages.empty());
}

int main() {
  test_small_blocks_fit_in_one_page();
  test_blocks_split_across_multiple_pages();
  test_oversized_single_block_alone_on_its_own_page_not_truncated();
  test_empty_input_yields_no_pages();

  printf("test_help_paginator: all tests passed\n");
  return 0;
}
