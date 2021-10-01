#include <stdexcept>

#include "zaf/message.hpp"
#include "zaf/zaf_exception.hpp"

namespace zaf {
Message::Message(const Actor& sender_actor, size_t code):
  sender_actor(sender_actor),
  code(code) {
}

size_t Message::get_code() const { return code; }

void Message::set_type(Message::Type t) { this->type = t; }

Message::Type Message::get_type() const { return type; }

const Actor& Message::get_sender_actor() const { return sender_actor; }

Message::~Message() {}

SerializedMessage::SerializedMessage(const Actor& sender_actor, size_t code):
  Message(sender_actor, code) {
}

bool SerializedMessage::is_serialized() const {
  return true;
}

void SerializedMessage::fill_with_element_addrs(std::vector<std::uintptr_t>&) const {
  throw ZAFException("SerializedMessage cannot provide addresses of the elements in the content.");
}

TypedSerializedMessage<std::vector<char>>::TypedSerializedMessage(
  const Actor& sender_actor, size_t code, Type type, size_t types_hash, std::vector<char>&& bytes, size_t offset):
  SerializedMessage(sender_actor, code),
  content_bytes(std::move(bytes)),
  offset(offset),
  types_hash(types_hash) {
  this->set_type(type);
}

SerializedMessage* TypedSerializedMessage<std::vector<char>>::serialize() const {
  return new TypedSerializedMessage<std::vector<char>> {
    this->get_sender_actor(),
    this->get_code(),
    this->get_type(),
    this->types_hash_code(),
    std::vector<char>{this->content_bytes}
  };
}

Deserializer TypedSerializedMessage<std::vector<char>>::make_deserializer() const {
  return {&content_bytes.front() + offset};
}

size_t TypedSerializedMessage<std::vector<char>>::types_hash_code() const {
  return types_hash;
}

TypedSerializedMessage<zmq::message_t>::TypedSerializedMessage(
  const Actor& sender_actor, size_t code, Type type, size_t types_hash, zmq::message_t&& bytes, size_t offset):
  SerializedMessage(sender_actor, code),
  message_bytes(std::move(bytes)),
  offset(offset),
  types_hash(types_hash) {
  this->set_type(type);
}

SerializedMessage* TypedSerializedMessage<zmq::message_t>::serialize() const {
  return new TypedSerializedMessage<zmq::message_t> {
    this->get_sender_actor(),
    this->get_code(),
    this->get_type(),
    this->types_hash_code(),
    zmq::message_t{
      (char*) this->message_bytes.data() + this->offset,
      this->message_bytes.size() - this->offset
    },
    0
  };
}

Deserializer TypedSerializedMessage<zmq::message_t>::make_deserializer() const {
  return {(char*) message_bytes.data() + offset};
}

size_t TypedSerializedMessage<zmq::message_t>::types_hash_code() const {
  return types_hash;
}

} // namespace zaf
