#pragma once

#include <array>
#include <stdint.h>
#include <vector>

#include "../events/EventLog.h"

constexpr size_t kMaxDeletableIdsPerPass = 256;  // sez. 9.3.1 - ~4 KB di RAM

// Sez. 9.3.1 - passata 1 dell'algoritmo di rotazione: raccoglie (in forma
// binaria a 16 byte) gli id degli eventi eliminabili - quelli con una riga
// END/INSTANT il cui ts e' anteriore al cutoff - fino a un tetto fisso.
// Progettata per essere alimentata riga per riga in streaming (observe()),
// senza mai richiedere l'intero file in memoria.
class DeletableIdCollector {
 public:
  explicit DeletableIdCollector(uint32_t cutoff, size_t cap = kMaxDeletableIdsPerPass);

  // Ignora silenziosamente la riga se il tetto e' gia' stato raggiunto, se
  // non e' una riga di chiusura (END/INSTANT), o se e' successiva al cutoff.
  void observe(const EventRecord& rec);

  bool capReached() const { return capReached_; }
  const std::vector<std::array<uint8_t, 16>>& ids() const { return ids_; }

  // Sez. 9.3.1 passata 2 - true se l'id (esadecimale) e' tra quelli raccolti.
  bool contains(const char* hexId) const;

 private:
  uint32_t cutoff_;
  size_t cap_;
  std::vector<std::array<uint8_t, 16>> ids_;
  bool capReached_ = false;
};
