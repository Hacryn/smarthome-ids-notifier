#pragma once

#include <stdint.h>

#include <vector>

#include "../events/EventTypes.h"
#include "../users/UserList.h"

// Sez. 6/7 - motore di notifica. Non testabile via harness host-side
// (dipende da LittleFS/Telegram reali); la logica pura che lo governa
// (NotificationPresentation, RetryTimer, NotificationFolder) e' testata
// separatamente.

// Sez. 6.1 - flusso normale: accoda un invio per ciascun utente idoneo
// (whitelist + filtro added_ts di sez. 4.6). Non bloccante: l'invio
// effettivo avviene nei cicli di loop successivi, al ritmo del rate
// limiter (sez. 6.6) tramite tickNotificationEngine. Da chiamare subito
// dopo aver scritto con successo la riga in log.jsonl.
void notifyEvent(const std::vector<AuthorizedUser>& users, const char* id, EventType type,
                  EventStatus status, uint32_t eventTs, bool eventApprox);

// Sez. 6.2/6.3/6.4/6.7 - scansione di recupero: da chiamare al boot, al
// ripristino della connettivita' dopo NETWORK_ISSUE, e (automaticamente,
// tramite tickNotificationEngine) allo scadere del timer di retry. Una
// richiesta ricevuta mentre una scansione e' gia' in corso viene scartata
// (sez. 6.3.1): la scansione in corso rilegge comunque lo stato completo.
void runRecoveryScan(const std::vector<AuthorizedUser>& users, uint32_t nowMillis,
                      uint32_t nowEpoch);

// Da chiamare ad ogni ciclo di loop: svuota la coda di invio al ritmo del
// rate limiter e fa scattare la scansione di recupero allo scadere del
// timer di retry o dopo un successo nel flusso normale (sez. 6.3).
void tickNotificationEngine(const std::vector<AuthorizedUser>& users, uint32_t nowMillis,
                             uint32_t nowEpoch);
