#pragma once

#include <memory>
#include <stdexcept>
#include <tuple>
#include <vector>

#include "actor.hpp"
#include "code.hpp"
#include "serializer.hpp"
#include "zaf_exception.hpp"

#include "zmq.hpp"

namespace zaf {
class MessageBody {
public:
  MessageBody(Code code);

  // the code of the message body
  size_t get_code() const;

  // the combination of the hash code of the content
  virtual size_t get_type_hash_code() const = 0;

  // whether this message is serialized
  virtual bool is_serialized() const = 0;

  // serialize this message body, including code, type hash and content
  virtual void serialize(Serializer& s) = 0;

  // only serialize the message content
  virtual void serialize_content(Serializer& s) = 0;

  virtual ~MessageBody() = default;

private:
  Code code;
};

class Message {
public:
  Message() = default;
  Message(const Message&) = delete;
  Message& operator=(const Message&) = delete;
  Message(Message&&) = default;
  Message& operator=(Message&&) = default;

  void set_body(MessageBody* body);
  void set_body(std::unique_ptr<MessageBody>&& body);
  void set_sender(const Actor& sender);

  MessageBody& get_body();
  const MessageBody& get_body() const;
  const Actor& get_sender() const;

private:
  Actor sender_actor = nullptr;
  std::unique_ptr<MessageBody> body = nullptr;
};

class MemoryMessageBody : public MessageBody {
public:
  MemoryMessageBody(Code code);

  bool is_serialized() const override;

  virtual void get_element_ptrs(std::vector<std::uintptr_t>& addrs) const = 0;
};

template<typename ContentTuple>
class TypedMessageBody : public MemoryMessageBody {
public:
  TypedMessageBody(Code code, ContentTuple&& content):
    MemoryMessageBody(code),
    content(std::move(content)) {
  }

  template<typename>
  struct TypesHashCode;

  template<typename ... ArgT>
  struct TypesHashCode<std::tuple<ArgT ...>> {
    inline const static size_t value = hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...);
  };

  size_t get_type_hash_code() const override {
    return TypesHashCode<ContentTuple>::value;
  }

  void get_element_ptrs(std::vector<std::uintptr_t>& addrs) const override {
    get_ptrs<0, std::tuple_size<ContentTuple>::value>(addrs);
  }

  template<typename>
  struct NonSerializableTypes;

  template<typename ... ArgT>
  struct NonSerializableTypes<std::tuple<ArgT ...>> {
    static std::string to_string() {
      return traits::NonSerializableAnalyzer<ArgT ...>::to_string();
    }
  };

  void serialize(Serializer& s) override {
    s.write(get_code())
     .write(get_type_hash_code());
    auto ptr = s.size();
    s.write(unsigned{0});
    if constexpr (traits::all_message_content_serializable<ContentTuple>::value) {
      serialize_content(s, std::make_index_sequence<std::tuple_size<ContentTuple>::value>{});
      s.move_write_ptr_to(ptr);
      s.write(static_cast<unsigned>(s.size() - ptr - sizeof(unsigned)));
      s.move_write_ptr_to_end();
    } else {
      throw ZAFException(__PRETTY_FUNCTION__, ": The TypedMessageBody "
        "contains non-serializable element(s): ",
        NonSerializableTypes<ContentTuple>::to_string());
    }
  }

  void serialize_content(Serializer& s) override {
    if constexpr (traits::all_message_content_serializable<ContentTuple>::value) {
      serialize_content(s,
        std::make_index_sequence<std::tuple_size<ContentTuple>::value>{});
    } else {
      throw ZAFException(__PRETTY_FUNCTION__, ": The TypedMessageBody "
        "contains non-serializable element(s): ",
        NonSerializableTypes<ContentTuple>::to_string());
    }
  }

private:
  template<size_t ... i>
  inline void serialize_content(Serializer& s, std::index_sequence<i ...>) const {
    s.write(std::get<i>(content) ...);
  }

  template<size_t i, size_t n>
  void get_ptrs(std::vector<std::uintptr_t>& addrs) const {
    if constexpr (i < n) {
      addrs[i] = reinterpret_cast<std::uintptr_t>(&std::get<i>(content));
      get_ptrs<i + 1, n>(addrs);
    }
  }

  // a std::tuple that contains multiple members
  ContentTuple content;
};

struct SerializedMessageBody : public MessageBody {
public:
  SerializedMessageBody(Code code);

  bool is_serialized() const override;

  template<typename ArgTypes>
  auto deserialize_content() {
    auto s = make_deserializer();
    return deserialize_content<ArgTypes, 0>(s);
  }

  virtual Deserializer make_deserializer() const = 0;

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
struct TypedSerializedMessageBody;

// Used in ActorBehavior
template<>
struct TypedSerializedMessageBody<std::vector<char>> : public SerializedMessageBody {
public:
  TypedSerializedMessageBody(Code code, size_t types_hash,
    std::vector<char>&& bytes, size_t offset = 0);

  Deserializer make_deserializer() const override;

  size_t get_type_hash_code() const override;

  void serialize(Serializer& s) override;

  void serialize_content(Serializer& s) override;

private:
  std::vector<char> content_bytes;
  const size_t offset;
  const size_t types_hash;
};

// Used in NetGate
template<>
struct TypedSerializedMessageBody<zmq::message_t> : public SerializedMessageBody {
public:
  TypedSerializedMessageBody(Code code, size_t types_hash,
    zmq::message_t&& bytes, size_t offset = 0);

  Deserializer make_deserializer() const override;

  size_t get_type_hash_code() const override;

  void serialize(Serializer& s) override;

  void serialize_content(Serializer& s) override;

private:
  zmq::message_t content_bytes;
  const size_t offset;
  const size_t types_hash;
};

template<typename ... ArgT>
auto make_message(const Actor& sender, Code code, ArgT&& ... args) {
  auto body = std::make_tuple(std::forward<ArgT>(args)...);
  using BodyType = decltype(body);
  Message message;
  message.set_sender(sender);
  message.set_body(std::make_unique<TypedMessageBody<BodyType>>(code, std::move(body)));
  return message;
}

template<typename ... ArgT>
auto new_message(const Actor& sender, Code code, ArgT&& ... args) {
  auto body = std::make_tuple(std::forward<ArgT>(args)...);
  using BodyType = decltype(body);
  auto message = new Message();
  message->set_sender(sender);
  message->set_body(std::make_unique<TypedMessageBody<BodyType>>(code, std::move(body)));
  return message;
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

Message make_serialized_message(Message& m);

Message* new_serialized_message(Message& m);

TypedSerializedMessageBody<std::vector<char>>
make_serialized_message(MessageBody& m);

TypedSerializedMessageBody<std::vector<char>>*
new_serialized_message(MessageBody& m);

void serialize(Serializer&, MessageBody*);

template<typename T,
  typename std::enable_if_t<std::is_same_v<T, MessageBody*>>* = nullptr>
T deserialize(Deserializer& s) {
  auto code = s.read<Code>();
  auto type_hash = s.read<size_t>();
  auto num_bytes = s.read<unsigned>();
  std::vector<char> bytes(num_bytes);
  s.read_bytes(&bytes.front(), num_bytes);
  return new TypedSerializedMessageBody<std::vector<char>>(
    code,
    type_hash,
    std::move(bytes)
  );
}
} // namespace zaf
