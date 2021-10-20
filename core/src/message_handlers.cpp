#include "zaf/message_handlers.hpp"

namespace zaf {
MessageHandlers::MessageHandlers(MessageHandlers&& other):
  handlers(std::move(other.handlers)) {
}

MessageHandlers& MessageHandlers::operator=(MessageHandlers&& other) {
  this->handlers = std::move(other.handlers);
  return *this;
}

bool MessageHandlers::try_process(Message& m) {
  auto iter = handlers.find(m.get_code());
  if (iter == handlers.end()) {
    return this->child
      ? this->child->try_process(m)
      : false;
  }
  iter->second->process(m);
  return true;
}

void MessageHandlers::process(Message& m) {
  if (!try_process(m)) {
    throw ZAFException(
      "Handler for code ", m.get_code(), " not found."
    );
  }
}

void MessageHandlers::add_handlers() {}

void MessageHandlers::add_child_handlers(MessageHandlers& child) {
  this->child = &child;
}

MessageHandlers& MessageHandlers::get_child_handlers() {
  return *this->child;
}

size_t MessageHandlers::size() const {
  return handlers.size();
}
} // namespace zaf
