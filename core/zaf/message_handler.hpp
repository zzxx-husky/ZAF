#pragma once

#include <memory>

#include "callable_signature.hpp"
#include "code.hpp"
#include "message.hpp"
#include "macros.hpp"
#include "zaf_exception.hpp"

namespace zaf {
// Store the type-erased user-defined message handler
struct MessageHandler {
  virtual void process(Message& m);
  virtual void process_body(MessageBody& body);
  virtual void process_body(MemoryMessageBody& body) = 0;
  virtual void process_body(SerializedMessageBody& body) = 0;

  virtual ~MessageHandler() = default;
};

template<typename Handler>
class RawMessageHandler : public MessageHandler {
private:
  Handler handler;

public:
  template<typename H>
  RawMessageHandler(H&& handler):
    handler(std::forward<H>(handler)) {
  }

  void process(Message& m) override {
    handler(m);
  }

  void process_body(MessageBody& body) override {
    throw ZAFException("RawMessageHandler does not process MessageBody.");
  }

  void process_body(MemoryMessageBody& body) override {
    throw ZAFException("RawMessageHandler does not process MemoryMessageBody.");
  }

  void process_body(SerializedMessageBody& body) override {
    throw ZAFException("RawMessageHandler does not process SerializedMessageBody.");
  }
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
  void process_body(MemoryMessageBody& body) override {
    if (body.get_type_hash_code() != ArgTypes::hash_code()) {
      throw ZAFException("The hash code of the message content types does not"
        " match with the argument types of the message handler.",
        " Expected: ", ArgTypes::hash_code(),
        " Actual: ", body.get_type_hash_code());
    }
    // no `auto` is allowed in the argument type, otherwise we need to
    // match the lambda with arguments in compile time.
    process_body(body, std::make_index_sequence<ArgTypes::size>());
  }

  void process_body(SerializedMessageBody& body) override {
    if (body.get_type_hash_code() != ArgTypes::hash_code()) {
      throw ZAFException("The hash code of the message content types does not"
        " match with the argument types of the message handler.",
        " Expected: ", ArgTypes::hash_code(),
        " Actual: ", body.get_type_hash_code());
    }
    // no `auto` is allowed in the argument type, otherwise we need to
    // match the lambda with arguments in compile time.
    if constexpr (traits::all_handler_arguments_serializable<ArgTypes>::value) {
      process_body(body, std::make_index_sequence<ArgTypes::size>());
    } else {
      throw ZAFException("The TypedMessageHandler contains non-serializable"
        " argument(s) but receives a serialized message.");
    }
  }

  template<size_t ... I>
  inline void process_body(MemoryMessageBody& m, std::index_sequence<I ...>) {
    m.get_element_ptrs(message_element_addrs);
    try {
      handler(
        static_cast<typename ArgTypes::template arg_t<I>>(
          *reinterpret_cast<typename ArgTypes::template decay_arg_t<I>*>(
            message_element_addrs.operator[](I)
          )
        )...
      );
    } catch (...) {
      std::throw_with_nested(ZAFException(
        "Exception caught in ", __PRETTY_FUNCTION__,
        " when handling typed message with code ", m.get_code()
      ));
    }
  }

  template<size_t ... I>
  inline void process_body(SerializedMessageBody& m, std::index_sequence<I ...>) {
    auto&& content = m.deserialize_content<ArgTypes>();
    try {
      handler(
        static_cast<typename ArgTypes::template arg_t<I>>(
          std::get<I>(content)
        )...
      );
    } catch (...) {
      std::throw_with_nested(ZAFException(
        "Exception caught in ", __PRETTY_FUNCTION__,
        " when handling serialized message with code ", m.get_code()
      ));
    }
  }
};

// create a pair that links the code with the user_handler
// while the type of the user_handler is erased.
template<typename Handler>
inline auto operator-(Code code, Handler&& user_handler) {
  using HandlerX = traits::remove_cvref_t<Handler>;
  MessageHandler* handler;
  if (code == DefaultCodes::DefaultMessageHandler) {
    if constexpr (std::is_invocable_v<Handler, Message&>) {
      handler = new RawMessageHandler<HandlerX>(std::forward<Handler>(user_handler));
    } else {
      throw ZAFException("Invalid message handler type for "
        "DefaultCodes::DefaultMessageHandler, which expects "
        "void(Message&).");
    }
  } else {
    handler = new TypedMessageHandler<HandlerX>(std::forward<Handler>(user_handler));
  }
  return std::make_pair(code.value, std::unique_ptr<MessageHandler>(handler));
}
} // namespace zaf
