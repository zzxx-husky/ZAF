#include "zaf/actor.hpp"
#include "zaf/message.hpp"

namespace zaf {
Message::Message(const Actor& sender):
  sender_actor(sender) {
}

const Actor& Message::get_sender() const {
  return sender_actor;
}
} // namespace zaf
