#pragma once

#include <stdint.h>

#include <vector>

#include "../users/UserList.h"
#include "RotationPolicy.h"

// Sez. 9 - orchestrazione di rotazione e sorveglianza dello spazio. Non
// testabile via harness host-side (dipende da LittleFS reale); la logica
// di soglia (RotationPolicy) e' testata separatamente.

// Sez. 9.4 - true se l'ultima verifica ha rilevato occupazione >= 95%
// (le scritture non essenziali, es. righe PENDING, vengono sospese altrove).
bool isFilesystemDegraded();

// Sez. 9 - punto di ingresso periodico: esegue la rotazione se dovuta per
// cadenza (sez. 9.2) o se lo spazio ha appena superato una soglia (sez.
// 9.4, rotazione anticipata), e invia le segnalazioni agli admin sui
// cambi di soglia. Il chiamante decide la cadenza con cui invocarla; non
// serve chiamarla ad ogni ciclo di loop (nessuna scansione file per
// decidere se la rotazione e' dovuta, sez. 9.2).
void performMaintenanceIfDue(const std::vector<AuthorizedUser>& users, uint32_t nowEpoch,
                              uint32_t retentionWeeks = kDefaultRetentionWeeks);

// Sez. 9.3.2 - rimuove i file temporanei residui di una rotazione
// interrotta da un blackout. Da chiamare una sola volta in setup().
void cleanupStaleRotationFiles(const std::vector<AuthorizedUser>& users);
