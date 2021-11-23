#pragma once

#include <variant>

#include "actor.hpp"
#include "message.hpp"
#include "message_bytes.hpp"

namespace zaf {
struct DelayedMessage {
  Actor receiver;
  std::variant<
    Message*,
    MessageBytes
  > message;

  DelayedMessage(const Actor& r, Message* m);
  DelayedMessage(const Actor& r, MessageBytes&& m);
  DelayedMessage(const DelayedMessage&) = delete;
  DelayedMessage(DelayedMessage&&);
  DelayedMessage& operator=(const DelayedMessage&) = delete;
  DelayedMessage& operator=(DelayedMessage&&);

  ~DelayedMessage();
};
} // namespace zaf
