#pragma once

#include <chrono>

#include "actor_behavior.hpp"

namespace zaf {
class ThreadBasedActorBehavior : public ActorBehavior {
public:
  void launch() override;
  // dont want to make receive / receive_once virtual, so override all of them.
  void receive(MessageHandlers&& handlers);
  void receive(MessageHandlers& handlers);
  void receive_once(MessageHandlers&& handlers);
  void receive_once(MessageHandlers& handlers);

protected:
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

  struct DelayedMessage {
    Actor receiver;
    std::variant<
      Message*,
      zmq::message_t
    > message;

    DelayedMessage(const Actor& r, Message* m);
    DelayedMessage(const Actor& r, zmq::message_t&& m);
    DelayedMessage(const DelayedMessage&) = default;
    DelayedMessage(DelayedMessage&&) = default;
    DelayedMessage& operator=(const DelayedMessage&) = default;
    DelayedMessage& operator=(DelayedMessage&&) = default;
  };

  std::optional<std::chrono::milliseconds> remaining_time_to_next_delayed_message() const;
  void flush_delayed_messages();

public:
  template<typename Rep, typename Period, typename ... ArgT>
  void delayed_send(const std::chrono::duration<Rep, Period>& delay, ActorBehavior& receiver,
    size_t code, ArgT&& ... args) {
    this->delayed_send(delay, LocalActorHandle{receiver.get_actor_id()}, code, std::forward<ArgT>(args) ...);
  }

  template<typename Rep, typename Period, typename ... ArgT>
  void delayed_send(const std::chrono::duration<Rep, Period>& delay, const Actor& receiver,
    size_t code, ArgT&& ... args) {
    if (!receiver) {
      return;
    }
    auto now = std::chrono::steady_clock::now();
    auto send_time = now + delay;
    receiver.visit(overloaded{
      [&](const LocalActorHandle& r) {
        delayed_messages.emplace(send_time, DelayedMessage(
          receiver,
          make_message(LocalActorHandle{this->get_actor_id()}, code, std::forward<ArgT>(args)...)
        ));
      },
      [&](const RemoteActorHandle& r) {
        if constexpr (traits::all_serializable<ArgT ...>::value) {
          std::vector<char> bytes;
          Serializer(bytes)
            .write(this->get_actor_id())
            .write(r.remote_actor_id)
            .write(code)
            .write(hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...))
            .write(std::forward<ArgT>(args) ...);
          delayed_messages.emplace(send_time, DelayedMessage(
            receiver,
            zmq::message_t{&bytes.front(), bytes.size()}
          ));
        } else {
          throw ZAFException("Attempt to serialize non-serializable data");
        }
      }
    });
  }

private:
  DefaultSortedMultiMap<TimePoint, DelayedMessage> delayed_messages;
};
} // namespace zaf
