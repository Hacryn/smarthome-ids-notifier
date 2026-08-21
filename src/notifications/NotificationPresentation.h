#pragma once

#include <stddef.h>
#include <stdint.h>

constexpr uint32_t kGracePeriodSec = 300;    // sez. 6.4 - default 5 minuti
constexpr uint32_t kAggregateThreshold = 3;  // sez. 6.7 - default
constexpr uint32_t kMaxRetries = 24;         // sez. 6.5 - default

struct RecoveryPresentation {
  bool isRecovered;  // prefisso esplicito di recupero
  bool isApprox;     // marcatura "~" sul timestamp
};

// Sez. 6.4 - decide se una notifica pendente va presentata come "recuperata".
// Un evento con timestamp approssimato (sez. 5.4) e' sempre trattato come
// recuperato/approssimato, indipendentemente dallo scarto temporale.
RecoveryPresentation decideRecoveryPresentation(uint32_t nowEpoch, uint32_t eventTs,
                                                 bool eventApprox, uint32_t gracePeriodSec);

// Sez. 6.7 - sopra soglia, le notifiche pendenti di un utente vanno
// raggruppate in un unico messaggio invece che inviate singolarmente.
bool shouldAggregate(size_t pendingCount, uint32_t threshold);

// Sez. 6.5 - true se il conteggio tentativi, dopo l'incremento per il
// fallimento corrente, supera il limite: la notifica va abbandonata.
bool exceedsMaxRetries(uint32_t attemptCountAfterFailure, uint32_t maxRetries);

// Sez. 7.2/8 - "un record PENDING che si avvicina a max_retries viene
// segnalato nel riepilogo periodico insieme agli eventi aperti": il design
// non specifica una soglia numerica, quindi ne fissiamo una esplicita qui
// (ultimi 3 tentativi disponibili) come scelta documentata, non implicita.
constexpr uint32_t kNearAbandonmentMargin = 3;
bool isNearAbandonment(uint32_t n, uint32_t maxRetries);
