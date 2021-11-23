#pragma once

#include <tuple>
#include <vector>

#include "zaf/code.hpp"
#include "zaf/message.hpp"
#include "zaf/message_body.hpp"

namespace zaf {
template<typename ... ArgT>
auto make_message(const Actor& sender, Code code, ArgT&& ... args) {
  auto body = std::make_tuple(std::forward<ArgT>(args)...);
  using BodyType = decltype(body);
  return TypedMessage<TypedMessageBody<BodyType>>(
    sender, code, std::move(body));
}

template<typename ... ArgT>
auto new_message(const Actor& sender, Code code, ArgT&& ... args) {
  auto body = std::make_tuple(std::forward<ArgT>(args)...);
  using BodyType = decltype(body);
  return new TypedMessage<TypedMessageBody<BodyType>>(
    sender, code, std::move(body));
}

template<typename ... ArgT>
auto make_message(Code code, ArgT&& ... args) {
  auto body = std::make_tuple(std::forward<ArgT>(args)...);
  using BodyType = decltype(body);
  return TypedMessageBody<BodyType>(code, std::move(body));
}

template<typename ... ArgT>
auto new_message(Code code, ArgT&& ... args) {
  auto body = std::make_tuple(std::forward<ArgT>(args)...);
  using BodyType = decltype(body);
  return new TypedMessageBody<BodyType>(code, std::move(body));
}

TypedMessage<TypedSerializedMessageBody<std::vector<char>>>
make_serialized_message(Message& m);

TypedMessage<TypedSerializedMessageBody<std::vector<char>>>*
new_serialized_message(Message& m);

TypedSerializedMessageBody<std::vector<char>>
make_serialized_message_body(MessageBody& m);

TypedSerializedMessageBody<std::vector<char>>*
new_serialized_message_body(MessageBody& m);

} // namespace zaf
