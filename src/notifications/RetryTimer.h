#pragma once

#include <stdint.h>

constexpr uint32_t kRetryIntervalMs = 60UL * 60UL * 1000UL;  // sez. 6.3 - default 60 minuti

// Sez. 6.3/6.3.1 - timer di retry non bloccante con protezione dalla
// rientranza durante una scansione di recupero.
class RetryTimer {
 public:
  // Sez. 6.3 - fallimento transitorio: avvia il timer se non attivo, o lo
  // resetta al valore pieno se gia' attivo (stessa operazione in entrambi i casi).
  void onTransientFailure(uint32_t nowMillis, uint32_t intervalMs);

  // Sez. 6.3 - successo nel flusso normale (mai durante una scansione, sez.
  // 6.3.1): ritorna true se va scatenata una scansione anticipata. Il timer
  // NON viene toccato qui: sara' la conclusione della scansione a deciderne
  // lo stato finale (endScan), con la stessa regola dello scadere naturale.
  bool onNormalFlowSuccess();

  bool isDue(uint32_t nowMillis) const;

  void beginScan();
  // allResolvedOrAbandoned: esito della scansione appena conclusa.
  void endScan(bool allResolvedOrAbandoned, uint32_t nowMillis, uint32_t intervalMs);

  bool scanInProgress() const { return scanInProgress_; }
  bool isActive() const { return active_; }

 private:
  bool active_ = false;
  uint32_t dueAtMillis_ = 0;
  bool scanInProgress_ = false;
};
