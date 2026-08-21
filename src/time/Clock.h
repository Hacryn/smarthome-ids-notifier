#pragma once

#include <stdint.h>

// Sez. 5.4/10.2/13 - orologio di sistema unico per tutto il firmware: NTP
// quando disponibile, ancora NVS come fallback prima del primo sync (o se
// l'orario torna implausibile). Non testabile via harness host-side
// (dipende da configTime()/time() reali); la soglia di plausibilita'
// (ClockPolicy) e' pura e testata a parte.

// Carica l'ancora persistita (sez. 5.4.1). Da chiamare una sola volta in
// setup(), prima di ogni uso di currentEpoch().
void initClock();

// Sez. 13 - avvia/ripete la sincronizzazione NTP in UTC (la conversione in
// ora locale avviene solo in visualizzazione, sez. 5.3/10.3, mai qui). Da
// richiamare ad ogni connessione WiFi riuscita, prima connessione compresa
// e dopo ogni riconnessione.
void beginNtpSync();

// Da chiamare ad ogni ciclo di loop: rileva il primo sync riuscito e
// persiste l'ancora NVS (subito dopo il sync, poi ogni 10 minuti mentre
// l'orario resta valido, sez. 5.4.1).
void tickClock(uint32_t nowMillis);

// Sez. 5.4.2 - true se l'orario corrente proviene da NTP (non dall'ancora
// stimata). Righe scritte mentre e' false vanno sempre marcate approssimate.
bool isTimeSynced();

// Sez. 3.3/5.4 - l'epoch corrente, da usare ovunque nel firmware al posto
// di calcoli manuali su millis()/ancora.
uint32_t currentEpoch();
