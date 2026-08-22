#include "SendOutcomeClassifier.h"

SendOutcomeCategory classifySendOutcome(const RawSendOutcome& outcome) {
  // isEmpty must be checked before isError: FastBot2's isError() only
  // compares the parsed "ok" field, which is false (not true) when no
  // response body was received at all (e.g. no network connectivity) - a
  // real success always has a body, so this ordering doesn't change any
  // previously-covered case.
  if (outcome.isEmpty) return SendOutcomeCategory::TRANSIENT_NETWORK;
  if (!outcome.isError) return SendOutcomeCategory::SUCCESS;

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
