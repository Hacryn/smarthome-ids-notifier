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
      // Code not covered by the table in sec. 6.5: treated as a system
      // error, never as a recipient failure (we don't abandon a send over
      // a code we don't know how to interpret).
      return SendOutcomeCategory::SYSTEM_ERROR;
  }
}
