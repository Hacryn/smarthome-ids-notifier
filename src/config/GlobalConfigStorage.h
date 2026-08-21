#pragma once

#include "GlobalConfig.h"

// Sez. 11.1 - persistenza NVS delle configurazioni globali. Non testabile
// via harness host-side (dipende da NVS reale).
GlobalConfig loadGlobalConfig();
void saveGlobalConfig(const GlobalConfig& cfg);

// Istanza condivisa in RAM (pattern gia' usato per fsErrorCounter()), cosi'
// i moduli che oggi leggono le costanti kXxx come default possono leggere
// il valore effettivo senza dover ricevere GlobalConfig come parametro
// esplicito in ogni firma. initGlobalConfigStore() va chiamata una sola
// volta in setup(); i comandi /setXxx (CommandRouter) aggiornano il campo
// e richiamano saveGlobalConfig(globalConfig()) per persisterlo.
GlobalConfig& globalConfig();
void initGlobalConfigStore();
