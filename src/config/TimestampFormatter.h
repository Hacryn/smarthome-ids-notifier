#pragma once

#include <stdint.h>

#include <string>

#include "UserConfig.h"

// Sez. 10.2/10.3 - formatta un epoch secondo il fuso orario (con gestione
// automatica del DST) e il formato data dell'utente. La variabile
// d'ambiente TZ e' globale al processo: viene impostata subito prima
// della conversione e ripristinata a UTC subito dopo (sez. 10.3), dato
// che questa e' l'unica funzione del firmware che la tocca. Con approx a
// true il risultato e' preceduto da "~" (sez. 5.4.2). Non testabile via
// harness host-side (tzset()/strftime() dipendono dalla libc del target).
std::string formatTimestampForUser(uint32_t epoch, const UserConfig& cfg, bool approx);
