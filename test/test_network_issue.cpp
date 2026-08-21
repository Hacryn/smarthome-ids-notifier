#include <cassert>
#include <cstdio>

#include "../src/NetworkIssueTracker.h"
#include "../src/WifiBackoff.h"

static void test_backoff_schedule() {
  assert(backoffDelayMs(1) == 5000);
  assert(backoffDelayMs(2) == 10000);
  assert(backoffDelayMs(3) == 20000);
  assert(backoffDelayMs(4) == 40000);
  assert(backoffDelayMs(5) == 80000);
  assert(backoffDelayMs(6) == 160000);
  assert(backoffDelayMs(7) == 300000);
  assert(backoffDelayMs(8) == 300000);  // tetto
  assert(backoffDelayMs(0) == 5000);    // trattato come 1
}

static void test_force_full_reconnect_every_ten_attempts() {
  for (uint32_t i = 1; i <= 9; i++) assert(!shouldForceFullReconnect(i));
  assert(shouldForceFullReconnect(10));
  for (uint32_t i = 11; i <= 19; i++) assert(!shouldForceFullReconnect(i));
  assert(shouldForceFullReconnect(20));
}

static void test_network_issue_blip_below_threshold_no_event() {
  NetworkIssueTracker tracker;
  NetworkIssueEvent ev = tracker.update(false, 1000, 1000, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::NONE);

  ev = tracker.update(true, 60000, 1059, 120);  // rientra dopo 59s, sotto soglia
  assert(ev.kind == NetworkIssueEvent::Kind::NONE);
}

static void test_network_issue_confirmed_after_threshold() {
  NetworkIssueTracker tracker;
  // t=0: cade la connettivita'.
  NetworkIssueEvent ev = tracker.update(false, 0, 1000, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::NONE);

  // t=119s: ancora sotto soglia.
  ev = tracker.update(false, 119000, 1119, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::NONE);

  // t=120s: soglia superata, START datato all'istante originale di caduta (epoch 1000).
  ev = tracker.update(false, 120000, 1120, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::STARTED);
  assert(ev.ts == 1000);

  // Ulteriori chiamate mentre e' ancora giu' non riemettono lo START.
  ev = tracker.update(false, 130000, 1130, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::NONE);

  // Ripristino: END con durata calcolata dai due epoch.
  ev = tracker.update(true, 200000, 1200, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::ENDED);
  assert(ev.ts == 1200);
  assert(ev.downDurationSec == 200);  // 1200 - 1000
}

static void test_network_issue_cycle_can_repeat() {
  NetworkIssueTracker tracker;
  tracker.update(false, 0, 1000, 120);
  tracker.update(false, 120000, 1120, 120);
  NetworkIssueEvent ev = tracker.update(true, 130000, 1130, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::ENDED);

  // Un secondo down/up deve funzionare identicamente (stato resettato correttamente).
  ev = tracker.update(false, 200000, 2000, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::NONE);
  ev = tracker.update(false, 320000, 2120, 120);
  assert(ev.kind == NetworkIssueEvent::Kind::STARTED);
  assert(ev.ts == 2000);
}

int main() {
  test_backoff_schedule();
  test_force_full_reconnect_every_ten_attempts();
  test_network_issue_blip_below_threshold_no_event();
  test_network_issue_confirmed_after_threshold();
  test_network_issue_cycle_can_repeat();

  printf("test_network_issue: tutti i test superati\n");
  return 0;
}
