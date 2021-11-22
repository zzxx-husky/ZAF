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
  if (iter != handlers.end()) {
    iter->second->process(body);
    return true;
  }
  if (this->child && this->child->try_process(body)) {
    return true;
  }
  return false;
}

bool MessageHandlers::try_process(Message& m) {
  auto iter = handlers.find(m.get_body().get_code());
  if (iter != handlers.end()) {
    iter->second->process(m.get_body());
    return true;
  }
  if (this->child && this->child->try_process(m)) {
    return true;
  }
  if (default_handler) {
    default_handler->process(m);
    return true;
  }
  return false;
}

void MessageHandlers::process(MessageBody& body) {
  if (!try_process(body)) {
    throw ZAFException(
      "Handler for code ", body.get_code(), " not found."
    );
  }
}

void MessageHandlers::process(Message& m) {
  if (!try_process(m)) {
    throw ZAFException(
      "Handler for code ", m.get_body().get_code(), " not found."
    );
  }
}

void MessageHandlers::add_handlers() {}

void MessageHandlers::add_child_handlers(MessageHandlers& child) {
  this->child = &child;
}

void MessageHandlers::remove_child_handlers() {
  this->child = nullptr;
}

MessageHandlers& MessageHandlers::get_child_handlers() {
  return *this->child;
}

size_t MessageHandlers::size() const {
  return handlers.size();
}
} // namespace zaf
