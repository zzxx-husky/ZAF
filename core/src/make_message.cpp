#include <vector>

#include "zaf/message.hpp"
#include "zaf/make_message.hpp"

namespace zaf {
TypedMessage<TypedSerializedMessageBody<std::vector<char>>>
make_serialized_message(Message& m) {
  auto& body = m.get_body();
  std::vector<char> bytes;
  Serializer s{bytes};
  body.serialize_content(s);
  return TypedMessage<TypedSerializedMessageBody<std::vector<char>>>(
    m.get_sender(),
    body.get_code(),
    body.get_type_hash_code(),
    std::move(bytes));
}

TypedMessage<TypedSerializedMessageBody<std::vector<char>>>*
new_serialized_message(Message& m) {
  auto& body = m.get_body();
  std::vector<char> bytes;
  Serializer s{bytes};
  body.serialize_content(s);
  return new TypedMessage<TypedSerializedMessageBody<std::vector<char>>>(
    m.get_sender(),
    body.get_code(),
    body.get_type_hash_code(),
    std::move(bytes));
}

TypedSerializedMessageBody<std::vector<char>>
make_serialized_message_body(MessageBody& m) {
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
new_serialized_message_body(MessageBody& m) {
  std::vector<char> bytes;
  Serializer s{bytes};
  m.serialize_content(s);
  return new TypedSerializedMessageBody<std::vector<char>>(
    m.get_code(),
    m.get_type_hash_code(),
    std::move(bytes)
  );
}
} // namespace zaf
