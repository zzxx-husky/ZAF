#pragma once

#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "actor.hpp"
#include "serializer.hpp"
#include "zaf_exception.hpp"

#include "zmq.hpp"

namespace zaf {
class SerializedMessage;

class Message {
public:
  enum Type : uint8_t {
    Normal = 0,
    Request = 1,
    Response = 2
  };

  Message(const Actor& sender_actor, size_t code);

  size_t get_code() const;

  const Actor& get_sender_actor() const;

  void set_type(Type);

  Type get_type() const;

  virtual size_t types_hash_code() const = 0;

  virtual bool is_serialized() const = 0;

  virtual void fill_with_element_addrs(std::vector<std::uintptr_t>& addrs) const = 0;

  virtual SerializedMessage* serialize() const = 0;

  virtual ~Message();

private:
  size_t code;
  Type type = Normal;
  Actor sender_actor;
};

template<typename MessageContent>
struct TypedMessage : public Message {
  TypedMessage(const Actor& sender_actor, size_t code, MessageContent&& content):
    Message(sender_actor, code),
    content(std::move(content)) {
  }

  bool is_serialized() const override {
    return false;
  }

  template<typename>
  struct Types;

  template<typename ... ArgT>
  struct Types<std::tuple<ArgT ...>> {
    inline const static size_t hash_code = hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...);
  };

  size_t types_hash_code() const override {
    return Types<MessageContent>::hash_code;
  }

  SerializedMessage* serialize() const override;

  void fill_with_element_addrs(std::vector<std::uintptr_t>& addrs) const {
    fill_addrs<0, std::tuple_size<MessageContent>::value>(addrs);
  }

private:
  template<size_t ... i>
  inline void serialize_elems(Serializer& s, std::index_sequence<i ...>) const {
    s.write(std::get<i>(content) ...);
  }

  template<size_t i, size_t n>
  void fill_addrs(std::vector<std::uintptr_t>& addrs) const {
    if constexpr (i < n) {
      addrs[i] = reinterpret_cast<std::uintptr_t>(&std::get<i>(content));
      fill_addrs<i + 1, n>(addrs);
    }
  }

  // a std::tuple that contains multiple members
  MessageContent content;
};

template<typename ... ArgT>
auto make_message(Actor sender, size_t code, ArgT&& ... args) {
  auto content = std::make_tuple(std::forward<ArgT>(args)...);
  return new TypedMessage<decltype(content)>(sender, code, std::move(content));
}

template<typename ... ArgT>
auto make_message(size_t code, ArgT&& ... args) {
  return make_message(Actor{}, code, std::forward<ArgT>(args) ...);
}

struct SerializedMessage : public Message {
public:
  SerializedMessage(const Actor& sender_actor, size_t code);

  bool is_serialized() const override;

  void fill_with_element_addrs(std::vector<std::uintptr_t>&) const override;

  virtual Deserializer make_deserializer() const = 0;

  template<typename ArgTypes>
  inline auto deseralize() {
    auto&& content = this->deserialize_content<ArgTypes>();
    auto m = new TypedMessage<decltype(content)>(
      this->get_sender_actor(), this->code, std::move(content));
    m->set_type(this->get_type());
    return m;
  }

  template<typename ArgTypes>
  auto deserialize_content() {
    auto s = make_deserializer();
    return deserialize_content<ArgTypes, 0>(s);
  }

private:
  template<typename ArgTypes, size_t i, typename ... ArgT>
  inline auto deserialize_content(Deserializer& s, ArgT&& ... args) {
    if constexpr (ArgTypes::size == i) {
      return std::make_tuple(std::forward<ArgT>(args)...);
    } else {
      return deserialize_content<ArgTypes, i + 1>(
        s, std::forward<ArgT>(args)...,
        deserialize<typename ArgTypes::template decay_arg_t<i>>(s)
      );
    }
  }
};

template<typename T>
struct TypedSerializedMessage;

template<>
struct TypedSerializedMessage<std::vector<char>> : public SerializedMessage {
public:
  TypedSerializedMessage(const Actor& sender_actor,
    size_t code, Type type, size_t types_hash, std::vector<char>&& bytes, size_t offset = 0);

  SerializedMessage* serialize() const override;

  Deserializer make_deserializer() const override;

  size_t types_hash_code() const override;

private:
  std::vector<char> content_bytes;
  const size_t offset;
  const size_t types_hash;
};

template<>
struct TypedSerializedMessage<zmq::message_t> : public SerializedMessage {
public:
  TypedSerializedMessage(const Actor& sender_actor,
    size_t code, Type type, size_t types_hash, zmq::message_t&& bytes, size_t offset = 0);

  SerializedMessage* serialize() const override;

  Deserializer make_deserializer() const override;

  size_t types_hash_code() const override;

private:
  zmq::message_t message_bytes;
  const size_t offset;
  const size_t types_hash;
};

template<typename MessageContent>
SerializedMessage* TypedMessage<MessageContent>::serialize() const {
  if constexpr (traits::all_message_content_serializable<MessageContent>::value) {
    std::vector<char> bytes;
    Serializer s(bytes);
    serialize_elems(s, std::make_index_sequence<std::tuple_size<MessageContent>::value>{});
    return new TypedSerializedMessage<std::vector<char>>{
      this->get_sender_actor(), this->get_code(), this->get_type(), this->types_hash_code(), std::move(bytes)
    };
  } else {
    throw ZAFException("The TypedMessage contains non-serializable element(s) but is required to be serialized.");
  }
}
} // namespace zaf
