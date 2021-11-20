#include "zaf/message.hpp"
#include "zaf/message_handler.hpp"

namespace zaf {
void MessageHandler::process(Message& m) {
  process(m.get_body());
}

void MessageHandler::process(MessageBody& body) {
  // no `auto` is allowed in the argument type, otherwise we need to
  // match the lambda with arguments in compile time.
  if (body.is_serialized()) {
    process(static_cast<SerializedMessageBody&>(body));
  } else {
    process(static_cast<MemoryMessageBody&>(body));
  }
}
} // namespace zaf
