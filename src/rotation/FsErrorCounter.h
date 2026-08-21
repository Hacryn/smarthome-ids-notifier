#pragma once

#include <stdint.h>

// Sez. 9.4 - contatore di errori di scrittura filesystem (dopo un retry
// gia' fallito). Nessuna dipendenza da Arduino/LittleFS: la classe e' pura
// e testabile; l'istanza condivisa sotto e' il punto di accesso usato dai
// moduli di scrittura reali.
class FsErrorCounter {
 public:
  // Ritorna true se questo e' il primo errore mai registrato (il chiamante
  // lo usa per decidere se notificare gli admin, sez. 9.4).
  bool recordFailure();
  uint32_t count() const { return count_; }

 private:
  uint32_t count_ = 0;
};

// Istanza globale condivisa da tutti i moduli di scrittura su LittleFS,
// esposta anche in /status (sez. 12.2).
FsErrorCounter& fsErrorCounter();
