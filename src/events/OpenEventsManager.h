#pragma once

#include <stdint.h>

#include <string>
#include <vector>

#include "../users/UserList.h"
#include "OpenEventsTracker.h"

// Sez. 8 - gestione degli eventi rimasti aperti dopo un riavvio inatteso.
// Non testabile via harness host-side (dipende da LittleFS/Telegram reali);
// la logica di rilevamento (OpenEventsTracker) e' testata separatamente.

// Legge l'intero log.jsonl e ritorna gli eventi ancora aperti.
std::vector<OpenEvent> detectOpenEvents();

// Sez. 8.1 - chiude un evento aperto: verifica che sia ancora aperto (id
// gia' chiuso o inesistente -> false, protezione da doppio click/comando
// duplicato), scrive la riga END e attiva il normale flusso di notifica
// (sez. 6.1).
bool closeOpenEvent(const std::vector<AuthorizedUser>& users, const std::string& id,
                     uint32_t closeTs, uint32_t& lastWrittenTs);

// Sez. 8 - invia il riepilogo degli eventi aperti (e delle notifiche
// PENDING vicine alla rinuncia, sez. 7.2) a tutti gli utenti autorizzati:
// con bottoni inline per chiudere per gli admin, senza per gli standard.
// Non invia nulla a un utente che non ha niente da vedere.
void sendOpenEventsSummary(const std::vector<AuthorizedUser>& users);
