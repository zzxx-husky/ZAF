#pragma once

#include <chrono>

#include "actor_behavior.hpp"

namespace zaf {
class WithDelayedSend {
public:
  WithDelayedSend(ActorBehavior& self);

  void launch();
  // dont want to make receive / receive_once virtual, so override all of them.
  void receive(MessageHandlers&& handlers);
  void receive(MessageHandlers& handlers);
  bool receive_once(MessageHandlers&& handlers);
  bool receive_once(MessageHandlers& handlers);

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
    this->delayed_send(delay, LocalActorHandle{receiver.get_actor_id()}, code, Message::Type::Normal, std::forward<ArgT>(args) ...);
  }

  template<typename Rep, typename Period, typename ... ArgT>
  void delayed_send(const std::chrono::duration<Rep, Period>& delay, ActorBehavior& receiver,
    size_t code, Message::Type type, ArgT&& ... args) {
    this->delayed_send(delay, LocalActorHandle{receiver.get_actor_id()}, code, type, std::forward<ArgT>(args) ...);
  }

  template<typename Rep, typename Period, typename ... ArgT>
  void delayed_send(const std::chrono::duration<Rep, Period>& delay, const Actor& receiver,
    size_t code, ArgT&& ... args) {
    this->delayed_send(delay, receiver, code, Message::Type::Normal, std::forward<ArgT>(args) ...);
  }

  template<typename Rep, typename Period, typename ... ArgT>
  void delayed_send(const std::chrono::duration<Rep, Period>& delay, const Actor& receiver,
    size_t code, Message::Type type, ArgT&& ... args) {
    if (!receiver) {
      return;
    }
    auto now = std::chrono::steady_clock::now();
    auto send_time = now + delay;
    receiver.visit(overloaded{
      [&](const LocalActorHandle& r) {
        auto m = make_message(LocalActorHandle{self.get_actor_id()}, code, std::forward<ArgT>(args)...);
        m->set_type(type);
        delayed_messages.emplace(send_time, DelayedMessage(receiver, m));
      },
      [&](const RemoteActorHandle& r) {
        if constexpr (traits::all_serializable<ArgT ...>::value) {
          std::vector<char> bytes;
          Serializer(bytes)
            .write(self.get_actor_id())
            .write(r.remote_actor_id)
            .write(code)
            .write(type)
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
  ActorBehavior& self;
};
} // namespace zaf
