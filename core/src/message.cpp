#include <stdexcept>

#include "zaf/message.hpp"
#include "zaf/zaf_exception.hpp"

namespace zaf {
void Message::set_body(MessageBody* body) {
  this->set_body(std::unique_ptr<MessageBody>(body));
}

void Message::set_body(std::unique_ptr<MessageBody>&& body) {
  this->body = std::move(body);
}

void Message::set_sender(const Actor& sender) {
  this->sender_actor = sender;
}

MessageBody& Message::get_body() {
  return *body;
}

const MessageBody& Message::get_body() const {
  return *body;
}

const Actor& Message::get_sender() const {
  return sender_actor;
}

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

Message make_serialized_message(Message& m) {
  Message message;
  message.set_sender(m.get_sender());
  auto& body = m.get_body();
  std::vector<char> bytes;
  Serializer s{bytes};
  body.serialize_content(s);
  message.set_body(new TypedSerializedMessageBody<std::vector<char>>(
    body.get_code(),
    body.get_type_hash_code(),
    std::move(bytes)
  ));
  return message;
}

Message* new_serialized_message(Message& m) {
  auto message = new Message();
  message->set_sender(m.get_sender());
  auto& body = m.get_body();
  std::vector<char> bytes;
  Serializer s{bytes};
  body.serialize_content(s);
  message->set_body(new TypedSerializedMessageBody<std::vector<char>>(
    body.get_code(),
    body.get_type_hash_code(),
    std::move(bytes)
  ));
  return message;
}

TypedSerializedMessageBody<std::vector<char>>
make_serialized_message(MessageBody& m) {
  std::vector<char> bytes;
  Serializer s{bytes};
  m.serialize_content(s);
  return TypedSerializedMessageBody<std::vector<char>>(
    m.get_code(),
    m.get_type_hash_code(),
    std::move(bytes)
  );
}

TypedSerializedMessageBody<std::vector<char>>*
new_serialized_message(MessageBody& m) {
  std::vector<char> bytes;
  Serializer s{bytes};
  m.serialize_content(s);
  return new TypedSerializedMessageBody<std::vector<char>>(
    m.get_code(),
    m.get_type_hash_code(),
    std::move(bytes)
  );
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
