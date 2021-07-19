#include "zaf/message_handler.hpp"

namespace zaf {
MessageHandler::~MessageHandler() {}

Code::Code(size_t value) : value(value) {}

MessageHandlers::MessageHandlers(MessageHandlers&& other):
  handlers(std::move(other.handlers)) {
}

MessageHandlers& MessageHandlers::operator=(MessageHandlers&& other) {
  this->handlers = std::move(other.handlers);
  return *this;
}
} // namespace zaf

