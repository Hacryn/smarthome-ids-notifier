#pragma once

// Sec. 6.5 - outcome categories for a Telegram send.
enum class SendOutcomeCategory {
  SUCCESS,
  TRANSIENT_NETWORK,    // connection failure: no JSON body received
  TRANSIENT_SERVER,     // HTTP 5xx
  THROTTLING,           // HTTP 429
  PERMANENT_RECIPIENT,  // HTTP 403/400
  SYSTEM_ERROR,         // HTTP 401/404, or any code not covered by the table
};

// Abstraction over fb::Result's raw outcome (sec. 3.5), to make
// classification testable without depending on FastBot2/network.
struct RawSendOutcome {
  bool isError;
  bool isEmpty;   // relevant only if isError == true
  int errorCode;  // relevant only if isError == true && isEmpty == false
};

SendOutcomeCategory classifySendOutcome(const RawSendOutcome& outcome);
