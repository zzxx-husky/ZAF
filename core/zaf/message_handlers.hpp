#pragma once

#include "code.hpp"
#include "message_handler.hpp"

namespace zaf {
class MessageHandlers {
public:
  template<typename ... ArgT>
  MessageHandlers(ArgT&& ... args) {
    add_handlers(std::forward<ArgT>(args) ...);
  }

  MessageHandlers(MessageHandlers&& other);

  MessageHandlers& operator=(MessageHandlers&& other);

  bool try_process(Message& m);
  bool try_process(MessageBody& m);

  void process(Message& m);
  void process(MessageBody& m);

  template<typename CodeHandler, typename ... ArgT>
  void add_handlers(CodeHandler&& code_handler, ArgT&& ... args) {
    if (!handlers.emplace(std::move(code_handler)).second) {
      throw ZAFException("Message handler code conflicts with a previous handler. Code: ", code_handler.first);
    }
    add_handlers(std::forward<ArgT>(args) ...);
  }

  template<typename CodeHandler, typename ... ArgT>
  void update_handlers(CodeHandler&& code_handler, ArgT&& ... args) {
    handlers[code_handler.first] = std::move(code_handler.second);
    update_handlers(std::forward<ArgT>(args) ...);
  }

  size_t size() const;

  void add_handlers();

  void add_child_handlers(MessageHandlers& child);

  MessageHandlers& get_child_handlers();

private:
  DefaultHashMap<size_t, std::unique_ptr<MessageHandler>> handlers;
  MessageHandlers* child = nullptr;
};
} // namespace zaf
