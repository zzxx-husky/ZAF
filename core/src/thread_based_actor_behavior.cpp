#include "zaf/thread_based_actor_behavior.hpp"

namespace zaf {
ThreadBasedActorBehavior::DelayedMessage::DelayedMessage(const Actor& r, Message* m):
  receiver(r),
  message(m) {
}

ThreadBasedActorBehavior::DelayedMessage::DelayedMessage(const Actor& r, zmq::message_t&& m):
  receiver(r),
  message(std::move(m)) {
}

void ThreadBasedActorBehavior::launch() {
  this->activate();
  this->start();
  if (this->is_activated()) {
    this->receive(this->behavior());
  }
  this->stop();
}

void ThreadBasedActorBehavior::receive_once(MessageHandlers&& handlers) {
  this->receive_once(handlers);
}

void ThreadBasedActorBehavior::receive_once(MessageHandlers& handlers) {
  constexpr std::chrono::milliseconds zero{0};
  while (true) {
    auto timeout = this->remaining_time_to_next_delayed_message();
    if (timeout) {
      if (this->ActorBehavior::receive_once(handlers, *timeout < zero ? zero : *timeout)) {
        return;
      }
      flush_delayed_messages();
    } else {
      this->ActorBehavior::receive_once(handlers);
      return;
    }
  }
}

void ThreadBasedActorBehavior::receive(MessageHandlers&& handlers) {
  this->receive(handlers);
}

void ThreadBasedActorBehavior::receive(MessageHandlers& handlers) {
  this->activate();
  while (this->is_activated()) {
    this->receive_once(handlers);
  }
}

std::optional<std::chrono::milliseconds> ThreadBasedActorBehavior::remaining_time_to_next_delayed_message() const {
  return delayed_messages.empty()
    ? std::nullopt
    : std::optional(std::chrono::duration_cast<std::chrono::milliseconds>(delayed_messages.begin()->first - std::chrono::steady_clock::now()));
}

void ThreadBasedActorBehavior::flush_delayed_messages() {
  while (!delayed_messages.empty() &&
         delayed_messages.begin()->first <= std::chrono::steady_clock::now()) {
    auto& msg = delayed_messages.begin()->second;
    std::visit(overloaded{
      [&](Message* m) {
        LocalActorHandle& r = msg.receiver;
        this->send(r, m);
      },
      [&](zmq::message_t& m) {
        RemoteActorHandle& r = msg.receiver;
        this->send(LocalActorHandle{r.net_sender_info->id}, DefaultCodes::ForwardMessage, std::move(m));
      }
    }, msg.message);
    delayed_messages.erase(delayed_messages.begin());
  }
}
} // namespace zaf
