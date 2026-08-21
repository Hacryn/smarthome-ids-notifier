#pragma once

#include "GlobalConfig.h"

// Sec. 11.1 - NVS persistence of the global configuration. Not testable via
// the host-side harness (depends on real NVS).
GlobalConfig loadGlobalConfig();
void saveGlobalConfig(const GlobalConfig& cfg);

// Shared in-RAM instance (same pattern already used for fsErrorCounter()),
// so modules that currently read the kXxx constants as defaults can read
// the effective value instead, without having to receive GlobalConfig as an
// explicit parameter in every signature. initGlobalConfigStore() must be
// called once in setup(); the /setXxx commands (CommandRouter) update the
// field and call saveGlobalConfig(globalConfig()) to persist it.
GlobalConfig& globalConfig();
void initGlobalConfigStore();
