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

  inline void process(Message& m) {
    try {
      handlers.at(m.get_code())->process(m);
    } catch (...) {
      if (handlers.count(m.get_code()) == 0) {
        std::throw_with_nested(ZAFException(
          "Handler for code ", m.get_code(), " not found."
        ));
      } else {
        throw;
      }
    }
  }

private:
  template<typename CodeHandler, typename ... ArgT>
  void add_handlers(CodeHandler&& code_handler, ArgT&& ... args) {
    if (!handlers.emplace(std::move(code_handler)).second) {
      throw ZAFException("Message handler code conflicts with a previous handler. Code: ", code_handler.first);
    }
    add_handlers(std::forward<ArgT>(args) ...);
  }

  inline void add_handlers() {}

  DefaultHashMap<size_t, std::unique_ptr<MessageHandler>> handlers;
};
} // namespace zaf
