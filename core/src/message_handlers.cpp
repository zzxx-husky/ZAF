#include "zaf/message_handlers.hpp"

namespace zaf {
MessageHandlers::MessageHandlers(MessageHandlers&& other):
  handlers(std::move(other.handlers)) {
}

MessageHandlers& MessageHandlers::operator=(MessageHandlers&& other) {
  this->handlers = std::move(other.handlers);
  return *this;
}
} // namespace zaf

