#include "zaf/with_delayed_send.hpp"

namespace zaf {
WithDelayedSend::WithDelayedSend(ActorBehavior& self):
  self(self) {
}

WithDelayedSend::DelayedMessage::DelayedMessage(const Actor& r, Message* m):
  receiver(r),
  message(m) {
}

WithDelayedSend::DelayedMessage::DelayedMessage(const Actor& r, zmq::message_t&& m):
  receiver(r),
  message(std::move(m)) {
}

void WithDelayedSend::launch() {
  self.activate();
  self.start();
  if (self.is_activated()) {
    this->receive(self.behavior());
  }
  self.stop();
}

bool WithDelayedSend::receive_once(MessageHandlers&& handlers) {
  return this->receive_once(handlers);
}

bool WithDelayedSend::receive_once(MessageHandlers& handlers) {
  constexpr std::chrono::milliseconds zero{0};
  while (true) {
    auto timeout = this->remaining_time_to_next_delayed_message();
    if (timeout) {
      auto ret = self.receive_once(handlers, *timeout < zero ? zero : *timeout);
      flush_delayed_messages();
      if (ret) {
        return true;
      }
    } else {
      return self.receive_once(handlers);
    }
  }
}

void WithDelayedSend::receive(MessageHandlers&& handlers) {
  this->receive(handlers);
}

void WithDelayedSend::receive(MessageHandlers& handlers) {
  self.activate();
  while (self.is_activated()) {
    this->receive_once(handlers);
  }
}

std::optional<std::chrono::milliseconds> WithDelayedSend::remaining_time_to_next_delayed_message() const {
  return delayed_messages.empty()
    ? std::nullopt
    : std::optional(std::chrono::duration_cast<std::chrono::milliseconds>(delayed_messages.begin()->first - std::chrono::steady_clock::now()));
}

void WithDelayedSend::flush_delayed_messages() {
  while (!delayed_messages.empty() &&
         delayed_messages.begin()->first <= std::chrono::steady_clock::now()) {
    auto& msg = delayed_messages.begin()->second;
    std::visit(overloaded{
      [&](Message* m) {
        LocalActorHandle& r = msg.receiver;
        self.send(r, m);
      },
      [&](zmq::message_t& m) {
        RemoteActorHandle& r = msg.receiver;
        self.send(LocalActorHandle{r.net_sender_info->id}, DefaultCodes::ForwardMessage, std::move(m));
      }
    }, msg.message);
    delayed_messages.erase(delayed_messages.begin());
  }
}
} // namespace zaf
