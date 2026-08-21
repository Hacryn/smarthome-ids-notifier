#include "SendOutcomeClassifier.h"

SendOutcomeCategory classifySendOutcome(const RawSendOutcome& outcome) {
  if (!outcome.isError) return SendOutcomeCategory::SUCCESS;
  if (outcome.isEmpty) return SendOutcomeCategory::TRANSIENT_NETWORK;

  switch (outcome.errorCode) {
    case 403:
    case 400:
      return SendOutcomeCategory::PERMANENT_RECIPIENT;
    case 401:
    case 404:
      return SendOutcomeCategory::SYSTEM_ERROR;
    case 429:
      return SendOutcomeCategory::THROTTLING;
    default:
      if (outcome.errorCode >= 500 && outcome.errorCode < 600) {
        return SendOutcomeCategory::TRANSIENT_SERVER;
      }
      // Codice non previsto dalla tabella di sez. 6.5: trattato come errore
      // di sistema, mai come fallimento del destinatario (non abbandoniamo
      // un invio per un codice che non sappiamo interpretare).
      return SendOutcomeCategory::SYSTEM_ERROR;
  }
}
