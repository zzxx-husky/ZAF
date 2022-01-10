#pragma once

#include <vector>
#include <type_traits>

#include "actor.hpp"
#include "hash.hpp"
#include "serializer.hpp"

namespace zaf {
struct MessageBytes {
  std::vector<char> header;
  std::vector<char> content;

  MessageBytes() = default;
  MessageBytes(const MessageBytes&) = delete;
  MessageBytes(MessageBytes&&) = default;
  MessageBytes& operator=(const MessageBytes&) = delete;
  MessageBytes& operator=(MessageBytes&&) = default;

  template<typename ... ArgT>
  static MessageBytes make(const LocalActorHandle& send,
    const LocalActorHandle& recv, Code code, ArgT&& ... args) {
    MessageBytes bytes;
    bytes.header.reserve(
      LocalActorHandle::SerializationSize +
      LocalActorHandle::SerializationSize +
      sizeof(code) +
      sizeof(size_t) +
      sizeof(unsigned)
    );
    const size_t type_hash = hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...);
    Serializer(bytes.header)
      .write(send)
      .write(recv)
      .write(code)
      .write(type_hash);
    Serializer(bytes.content)
      .write(std::forward<ArgT>(args) ...);
    Serializer(bytes.header)
      .write(static_cast<unsigned>(bytes.content.size()));
    return bytes;
  }
};
} // namespace zaf
