#pragma once

#include <type_traits>

#include "actor.hpp"
#include "message_body.hpp"

namespace zaf {
class Message {
public:
  Message(const Actor& sender);

  const Actor& get_sender() const;

  virtual MessageBody& get_body() = 0;
  virtual const MessageBody& get_body() const = 0;

  virtual ~Message() = default;

private:
  const Actor sender_actor = nullptr;
};

template<typename Body>
class TypedMessage : public Message, public Body {
public:
  static_assert(std::is_base_of_v<MessageBody, Body>,
    "The Body part of TypedMessage must be a subclass of MessageBody.");

  template<typename ... ArgT>
  TypedMessage(const Actor& sender, Code code, ArgT&& ... args):
    Message(sender),
    Body(code, std::forward<ArgT>(args) ...) {
  }

  MessageBody& get_body() {
    return *this;
  }

  const MessageBody& get_body() const {
    return *this;
  }
};
} // namespace zaf
