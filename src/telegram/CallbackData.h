#pragma once

#include <string>

// Sez. 8.1 - formato "c:<id>" (34 byte, entro il limite di 64 byte imposto
// da Telegram al callback_data).
std::string closeEventCallbackData(const char* id);

// Ritorna false se il prefisso non e' "c:" o l'id non e' lungo 32 caratteri.
bool parseCloseEventCallbackData(const std::string& data, std::string& outId);
