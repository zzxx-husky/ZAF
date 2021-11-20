#include "zaf/message_handlers.hpp"

namespace zaf {
MessageHandlers::MessageHandlers(MessageHandlers&& other):
  handlers(std::move(other.handlers)) {
}

MessageHandlers& MessageHandlers::operator=(MessageHandlers&& other) {
  this->handlers = std::move(other.handlers);
  return *this;
}

bool MessageHandlers::try_process(MessageBody& body) {
  auto iter = handlers.find(body.get_code());
  if (iter == handlers.end()) {
    return this->child
      ? this->child->try_process(body)
      : false;
  }
  iter->second->process(body);
  return true;
}

bool MessageHandlers::try_process(Message& m) {
  return this->try_process(m.get_body());
}

void MessageHandlers::process(MessageBody& body) {
  if (!try_process(body)) {
    throw ZAFException(
      "Handler for code ", body.get_code(), " not found."
    );
  }
}

void MessageHandlers::process(Message& m) {
  this->process(m.get_body());
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
