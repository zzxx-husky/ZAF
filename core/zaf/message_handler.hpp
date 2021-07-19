#pragma once

#include <memory>
#include <type_traits>

#include "callable_signature.hpp"
#include "message.hpp"
#include "macros.hpp"

#include "glog/logging.h"

namespace zaf {
// Store the type-erased user-defined message handler
struct MessageHandler {
  virtual void process(Message&) = 0;
  virtual ~MessageHandler();
};

// Store the original user-defined message handler
// and process the messages with the handler
template<typename Handler>
class TypedMessageHandler : public MessageHandler {
private:
  Handler handler;
  using ArgTypes = typename traits::is_callable<Handler>::args_t;
  std::vector<std::uintptr_t> message_element_addrs;

public:
  template<typename H>
  TypedMessageHandler(H&& handler): handler(std::forward<H>(handler)) {
    static_assert(traits::is_callable<Handler>::value,
      " Not an acceptable handler.");
    message_element_addrs.resize(ArgTypes::size);
  }

  // recover the types of the arguments in the message
  // then forward the arguments into the handler
  void process(Message& m) override {
    // no `auto` is allowed in the argument type, otherwise we need to match the lambda with arguments in compile time.
    invoke(m, std::make_index_sequence<ArgTypes::size>());
  }

  template<size_t ... I>
  inline void invoke(Message& m, std::index_sequence<I ...>) {
    m.fill_with_element_addrs(message_element_addrs);
    try {
      handler(
        static_cast<typename ArgTypes::template arg_t<I>>(
          *reinterpret_cast<typename ArgTypes::template decay_arg_t<I>*>(
            message_element_addrs.operator[](I)
          )
        )...
      );
    } catch (...) {
      LOG(ERROR) << "Exception caught in " << __PRETTY_FUNCTION__ << " when handling message with code " << m.get_code();
      throw;
    }
  }
};

struct Code {
  size_t value;

  Code(size_t value);

  // create a pair that links the code with the user_handler
  // while the type of the user_handler is erased.
  template<typename Handler>
  inline friend auto operator-(Code code, Handler&& user_handler) {
    // return as it is
    using HandlerX = std::remove_cv_t<std::remove_reference_t<Handler>>;
    MessageHandler* handler = new TypedMessageHandler<HandlerX>(std::forward<Handler>(user_handler));
    return std::make_pair(code.value, std::unique_ptr<MessageHandler>(handler));
  }

  inline operator size_t() const {
    return value;
  }
};

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
      LOG_IF(ERROR, handlers.count(m.get_code()) == 0) <<
        "Handler for code " << m.get_code() << " not found.";
      throw;
    }
  }

private:
  template<typename CodeHandler, typename ... ArgT>
  void add_handlers(CodeHandler&& code_handler, ArgT&& ... args) {
    handlers.emplace(std::move(code_handler));
    add_handlers(std::forward<ArgT>(args) ...);
  }

  inline void add_handlers() {}

  DefaultHashMap<size_t, std::unique_ptr<MessageHandler>> handlers;
};

} // namespace zaf
