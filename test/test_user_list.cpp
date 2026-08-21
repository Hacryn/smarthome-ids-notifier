#include <cassert>
#include <cstdio>

#include "../src/users/UserList.h"

static void test_is_authorized_and_is_admin() {
  std::vector<AuthorizedUser> users = {{111, true, 1000}, {222, false, 2000}};

  assert(isAuthorized(users, 111));
  assert(isAuthorized(users, 222));
  assert(!isAuthorized(users, 333));

  assert(isAdmin(users, 111));
  assert(!isAdmin(users, 222));
  assert(!isAdmin(users, 333));  // not authorized -> never admin
}

static void test_add_user_rejects_duplicate() {
  std::vector<AuthorizedUser> users;
  assert(addUser(users, 111, false, 1000));
  assert(!addUser(users, 111, true, 2000));  // already present
  assert(users.size() == 1);
  assert(!isAdmin(users, 111));  // the second addUser changed nothing
}

static void test_remove_user() {
  std::vector<AuthorizedUser> users = {{111, false, 1000}};
  assert(removeUser(users, 111));
  assert(!isAuthorized(users, 111));
  assert(!removeUser(users, 111));  // already removed
}

static void test_set_admin_flag() {
  std::vector<AuthorizedUser> users = {{111, false, 1000}};
  assert(setAdminFlag(users, 111, true));
  assert(isAdmin(users, 111));
  assert(setAdminFlag(users, 111, false));
  assert(!isAdmin(users, 111));
  assert(!setAdminFlag(users, 999, true));  // not present
}

static void test_reset_users() {
  std::vector<AuthorizedUser> users = {{111, true, 1000}, {222, false, 2000}};
  resetUsers(users);
  assert(users.empty());
}

static void test_ensure_onboarding_admin_only_when_empty() {
  std::vector<AuthorizedUser> users;
  assert(ensureOnboardingAdmin(users, 111111111LL, 1755000000));
  assert(users.size() == 1);
  assert(isAdmin(users, 111111111LL));

  // Whitelist already populated: onboarding must not act again.
  assert(!ensureOnboardingAdmin(users, 999999999LL, 1755000001));
  assert(!isAuthorized(users, 999999999LL));
}

static void test_serialize_roundtrip() {
  std::vector<AuthorizedUser> users = {
      {111111111LL, true, 1755000000},
      {-1001234567890LL, false, 1755600000},  // sec. 4.2 - group id, negative
  };

  std::string json = serializeUsers(users);

  std::vector<AuthorizedUser> parsed;
  assert(parseUsers(json, parsed));
  assert(parsed.size() == 2);
  assert(parsed[0].chatId == 111111111LL);
  assert(parsed[0].admin == true);
  assert(parsed[0].addedTs == 1755000000);
  assert(parsed[1].chatId == -1001234567890LL);
  assert(parsed[1].admin == false);
}

static void test_parse_rejects_malformed_and_missing_field() {
  std::vector<AuthorizedUser> out;
  assert(!parseUsers("not json", out));
  assert(!parseUsers("{\"authorized\":[{\"chat_id\":111,\"admin\":true}]}", out));  // missing added_ts
}

static void test_parse_empty_authorized_array() {
  std::vector<AuthorizedUser> out;
  assert(parseUsers("{\"authorized\":[]}", out));
  assert(out.empty());
}

int main() {
  test_is_authorized_and_is_admin();
  test_add_user_rejects_duplicate();
  test_remove_user();
  test_set_admin_flag();
  test_reset_users();
  test_ensure_onboarding_admin_only_when_empty();
  test_serialize_roundtrip();
  test_parse_rejects_malformed_and_missing_field();
  test_parse_empty_authorized_array();

  printf("test_user_list: all tests passed\n");
  return 0;
}
