#pragma once

#include "code.hpp"
#include "message_handler.hpp"

namespace zaf {
class MessageHandlers {
public:
  template<typename ... ArgT>
  MessageHandlers(ArgT&& ... args):
    default_handler(nullptr) {
    add_handlers(std::forward<ArgT>(args) ...);
  }

  MessageHandlers(MessageHandlers&& other);

  MessageHandlers& operator=(MessageHandlers&& other);

  bool try_process(Message& m);
  bool try_process_body(MessageBody& m);

  void process(Message& m);
  void process_body(MessageBody& m);

  template<typename CodeHandler, typename ... ArgT>
  void add_handlers(CodeHandler&& code_handler, ArgT&& ... args) {
    if (code_handler.first == DefaultCodes::DefaultMessageHandler) {
      if (default_handler) {
        throw ZAFException("Default message handler already exists.");
      }
      default_handler = std::move(code_handler.second);
    } else {
      if (!handlers.emplace(std::move(code_handler)).second) {
        throw ZAFException("Message handler code conflicts with a previous "
          "handler. Code: ", code_handler.first);
      }
    }
    add_handlers(std::forward<ArgT>(args) ...);
  }

  template<typename CodeHandler, typename ... ArgT>
  void update_handlers(CodeHandler&& code_handler, ArgT&& ... args) {
    if (code_handler.first == DefaultCodes::DefaultMessageHandler) {
      default_handler = std::move(code_handler.second);
    } else {
      handlers[code_handler.first] = std::move(code_handler.second);
    }
    update_handlers(std::forward<ArgT>(args) ...);
  }

  size_t size() const;

  void add_handlers();

  void add_child_handlers(MessageHandlers& child);
  MessageHandlers& get_child_handlers();
  void remove_child_handlers();

private:
  DefaultHashMap<size_t, std::unique_ptr<MessageHandler>> handlers;
  MessageHandlers* child = nullptr;
  std::unique_ptr<MessageHandler> default_handler = nullptr;
};
} // namespace zaf
