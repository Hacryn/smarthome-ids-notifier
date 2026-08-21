#pragma once

// Sez. 6.5 - categorie di esito di un invio Telegram.
enum class SendOutcomeCategory {
  SUCCESS,
  TRANSIENT_NETWORK,    // fallimento di connessione: nessun corpo JSON ricevuto
  TRANSIENT_SERVER,     // HTTP 5xx
  THROTTLING,           // HTTP 429
  PERMANENT_RECIPIENT,  // HTTP 403/400
  SYSTEM_ERROR,         // HTTP 401/404, o qualunque codice non previsto dalla tabella
};

// Astrazione dell'esito grezzo di fb::Result (sez. 3.5), per rendere la
// classificazione testabile senza dipendere da FastBot2/rete.
struct RawSendOutcome {
  bool isError;
  bool isEmpty;   // rilevante solo se isError == true
  int errorCode;  // rilevante solo se isError == true && isEmpty == false
};

SendOutcomeCategory classifySendOutcome(const RawSendOutcome& outcome);
