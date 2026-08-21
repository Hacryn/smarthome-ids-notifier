#pragma once

#include <stdint.h>

// Sez. 3.4.1 - macchina a stati pura per la rilevazione di NETWORK_ISSUE.
// Riceve un segnale generico di raggiungibilita' (nella fase attuale: solo
// lo stato WiFi; la sez. 3.4.1 richiede anche la raggiungibilita' delle API
// Telegram, che verra' combinata qui una volta introdotto il client - fase 6).
struct NetworkIssueEvent {
  enum class Kind { NONE, STARTED, ENDED } kind = Kind::NONE;
  uint32_t ts = 0;              // istante del rilevamento (START) o del ripristino (END)
  uint32_t downDurationSec = 0;  // valorizzato solo per ENDED
};

class NetworkIssueTracker {
 public:
  // Da chiamare periodicamente. epochNow e' la stima corrente dell'epoch,
  // usata per datare lo START (al momento in cui la irraggiungibilita' e'
  // iniziata, non a quando la soglia viene superata) e l'END.
  NetworkIssueEvent update(bool reachable, uint32_t nowMillis, uint32_t epochNow,
                            uint32_t thresholdSec);

 private:
  bool unreachable_ = false;
  bool confirmed_ = false;  // soglia gia' superata, START gia' emesso
  uint32_t unreachableSinceMillis_ = 0;
  uint32_t unreachableSinceEpoch_ = 0;
};
