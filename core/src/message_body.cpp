#include <vector>

#include "zaf/code.hpp"
#include "zaf/message_body.hpp"
#include "zaf/serializer.hpp"

#include "zmq.hpp"

namespace zaf {
MessageBody::MessageBody(Code code):
  code(code) {
}

size_t MessageBody::get_code() const {
  return code;
}

MemoryMessageBody::MemoryMessageBody(Code code):
  MessageBody(code) {
}

bool MemoryMessageBody::is_serialized() const {
  return false;
}

SerializedMessageBody::SerializedMessageBody(Code code):
  MessageBody(code) {
}

bool SerializedMessageBody::is_serialized() const {
  return true;
}

TypedSerializedMessageBody<std::vector<char>>::TypedSerializedMessageBody(
  Code code, size_t types_hash, std::vector<char>&& bytes, size_t offset):
  SerializedMessageBody(code),
  content_bytes(std::move(bytes)),
  offset(offset),
  types_hash(types_hash) {
}

Deserializer TypedSerializedMessageBody<std::vector<char>>::make_deserializer() const {
  return {&content_bytes.front() + offset};
}

size_t TypedSerializedMessageBody<std::vector<char>>::get_type_hash_code() const {
  return types_hash;
}

void TypedSerializedMessageBody<std::vector<char>>::serialize(Serializer& s) {
  s.write(get_code())
   .write(types_hash)
   .write(static_cast<unsigned>(content_bytes.size() - offset))
   .write_bytes(&content_bytes.front() + offset, content_bytes.size() - offset);
}

void TypedSerializedMessageBody<std::vector<char>>::serialize_content(Serializer& s) {
  s.write_bytes(&content_bytes.front() + offset, content_bytes.size() - offset);
}

TypedSerializedMessageBody<zmq::message_t>::TypedSerializedMessageBody(
  Code code, size_t types_hash, zmq::message_t&& bytes, size_t offset):
  SerializedMessageBody(code),
  content_bytes(std::move(bytes)),
  offset(offset),
  types_hash(types_hash) {
}

Deserializer TypedSerializedMessageBody<zmq::message_t>::make_deserializer() const {
  return {content_bytes.data<char>() + offset};
}

size_t TypedSerializedMessageBody<zmq::message_t>::get_type_hash_code() const {
  return types_hash;
}

void TypedSerializedMessageBody<zmq::message_t>::serialize(Serializer& s) {
  s.write(get_code())
   .write(types_hash)
   .write(static_cast<unsigned>(content_bytes.size() - offset))
   .write_bytes(content_bytes.data<char>() + offset, content_bytes.size() - offset);
}

void TypedSerializedMessageBody<zmq::message_t>::serialize_content(Serializer& s) {
  s.write_bytes(content_bytes.data<char>() + offset, content_bytes.size() - offset);
}

void serialize(Serializer& s, MessageBody* body) {
  s.write(body->get_code())
   .write(body->get_type_hash_code());
  auto ptr = s.size();
  s.write(unsigned(0));
  body->serialize_content(s);
  s.move_write_ptr_to(ptr);
  s.write(static_cast<unsigned>(s.size() - ptr - sizeof(unsigned)));
  s.move_write_ptr_to_end();
}
} // namespace zaf
