#include <utility>

#include "zaf/actor.hpp"
#include "zaf/delayed_message.hpp"
#include "zaf/message.hpp"
#include "zaf/message_bytes.hpp"
#include "zaf/traits.hpp"

namespace zaf {
DelayedMessage::DelayedMessage(const Actor& r, Message* m):
  receiver(r),
  message(m) {
}

DelayedMessage::DelayedMessage(const Actor& r, MessageBytes&& m):
  receiver(r),
  message(std::move(m)) {
}

DelayedMessage::DelayedMessage(DelayedMessage&& m):
  receiver(std::move(m.receiver)) {
  std::visit(overloaded {
    [&](Message*& msg) {
      this->message = msg;
      msg = nullptr;
    },
    [&](MessageBytes& bytes) {
      this->message = std::move(bytes);
    }
  }, m.message);
}

DelayedMessage& DelayedMessage::operator=(DelayedMessage&& m) {
  this->receiver = std::move(m.receiver);
  std::visit(overloaded {
    [&](Message*& msg) {
      this->message = msg;
      msg = nullptr;
    },
    [&](MessageBytes& bytes) {
      this->message = std::move(bytes);
    }
  }, m.message);
  return *this;
}

DelayedMessage::~DelayedMessage() {
  std::visit(overloaded {
    [&](Message* m) {
      if (m) {
        delete m;
      }
    },
    [&](auto&&) {}
  }, message);
}


} // namespace zaf
