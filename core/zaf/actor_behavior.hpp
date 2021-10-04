#pragma once

#include <cstdint>
#include <deque>

#include "actor.hpp"
#include "message_handlers.hpp"
#include "zaf_exception.hpp"

#include "zmq.hpp"

namespace zaf {
class ActorSystem;
class ActorGroup;
class ActorBehavior;

// make all `send` invocations to `request`
class Requester {
public:
  ActorBehavior& self;

  template<typename ... ArgT>
  decltype(auto) send(ArgT&& ... args);

  template<typename ... ArgT>
  decltype(auto) request(ArgT&& ... args);
};

class ActorBehavior {
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

public:
  ActorBehavior() = default;

  /**
   * To be overrided by subclass
   **/
  virtual void start();
  virtual void stop();
  virtual MessageHandlers behavior();

  // send a normal message to message sender
  template<typename ... ArgT>
  inline void reply(size_t code, ArgT&& ... args) {
    auto type = this->get_current_message().get_type() == Message::Type::Request
      ? Message::Type::Response
      : Message::Type::Normal;
    this->send(this->get_current_message().get_sender_actor(),
      code, type, std::forward<ArgT>(args)...);
  }

  // send a message to a ActorBehavior
  template<typename ... ArgT>
  inline void send(ActorBehavior& receiver, size_t code, ArgT&& ... args) {
    this->send(LocalActorHandle{receiver.get_actor_id()},
      code, Message::Type::Normal, std::forward<ArgT>(args)...);
  }

  template<typename ... ArgT>
  inline void send(ActorBehavior& receiver, size_t code, Message::Type type, ArgT&& ... args) {
    this->send(receiver.get_actor_id(), code, type, std::forward<ArgT>(args)...);
  }

  // send a message to Actor
  template<typename ... ArgT>
  inline void send(const Actor& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver, code, Message::Type::Normal, std::forward<ArgT>(args) ...);
  }

  template<typename ... ArgT>
  void send(const Actor& receiver, size_t code, Message::Type type, ArgT&& ... args);

  // send a message to LocalActorHandle
  template<typename ... ArgT>
  inline void send(const LocalActorHandle& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver, code, Message::Type::Normal, std::forward<ArgT>(args) ...);
  }

  template<typename ... ArgT>
  inline void send(const LocalActorHandle& receiver, size_t code, Message::Type type, ArgT&& ... args) {
    auto m = make_message(LocalActorHandle{this->get_actor_id()}, code, std::forward<ArgT>(args)...);
    m->set_type(type);
    this->send(receiver, m);
  }

  void send(const LocalActorHandle& receiver, Message* m);

  // to process one incoming message with message handlers
  bool receive_once(MessageHandlers&& handlers, bool non_blocking = false);
  bool receive_once(MessageHandlers& handlers, bool non_blocking = false);
  bool receive_once(MessageHandlers&& handlers, const std::chrono::milliseconds&);
  bool receive_once(MessageHandlers& handlers, const std::chrono::milliseconds&);
  bool receive_once(MessageHandlers&& handlers, long timeout);
  bool receive_once(MessageHandlers& handlers, long timeout);
  void receive(MessageHandlers&& handlers);
  void receive(MessageHandlers& handlers);

  template<typename Callback,
    typename std::enable_if_t<std::is_invocable_v<Callback, Message*>>* = nullptr>
  inline void receive(Callback&& callback) {
    this->activate();
    while (this->is_activated()) {
      this->receive_once(callback);
    }
  }

  // timeout == 0, non-blocking
  // timeout == -1, block until receiving a message
  // timeout > 0, wait for specified timeout or until receiving a message
  template<typename Callback,
    typename std::enable_if_t<std::is_invocable_v<Callback, Message*>>* = nullptr>
  bool receive_once(Callback&& callback, long timeout = -1);

  template<typename Callback,
    typename std::enable_if_t<std::is_invocable_v<Callback, Message*>>* = nullptr>
  bool inner_receive_once(Callback&& callback, long timeout = -1);

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
    size_t code, Message::Type type, ArgT&& ... args);

  struct RequestHelper {
    ActorBehavior& self;
    void on_reply(MessageHandlers&& handlers);
    void on_reply(MessageHandlers& handlers);
  };

  template<typename Receiver, typename ... ArgT>
  inline RequestHelper request(Receiver&& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver, code, Message::Type::Request, std::forward<ArgT>(args) ...);
    return RequestHelper{*this};
  }


  ActorSystem& get_actor_system();
  ActorGroup& get_actor_group();
  Actor get_self_actor();
  Message& get_current_message() const;
  Actor get_current_sender_actor() const;

  void activate();
  void deactivate();
  bool is_activated() const;

  /**
   * To be used by ZAF
   **/
  virtual void initialize_actor(ActorSystem& sys, ActorGroup& group);
  virtual void terminate_actor();

  void initialize_routing_id_buffer();

  virtual void initialize_recv_socket();
  virtual void terminate_recv_socket();

  virtual void initialize_send_socket();
  virtual void terminate_send_socket();

  zmq::socket_t& get_recv_socket();
  zmq::socket_t& get_send_socket();

  virtual void launch();

  virtual ~ActorBehavior();

  ActorIdType get_actor_id() const;

protected:
  void connect(ActorIdType peer);
  void disconnect(ActorIdType peer);

  std::optional<std::chrono::milliseconds> remaining_time_to_next_delayed_message() const;
  void flush_delayed_messages();

  // time to send -> delayed message
  DefaultSortedMultiMap<TimePoint, DelayedMessage> delayed_messages;

  std::string routing_id_buffer;
  const std::string& get_routing_id(ActorIdType id, bool send_or_recv);

  DefaultHashSet<ActorIdType> connected_receivers;
  bool activated = false;
  Message* current_message = nullptr;

  int waiting_for_response = 0;
  std::deque<Message*> pending_messages;

  ActorIdType actor_id;
  zmq::socket_t send_socket, recv_socket;
  ActorGroup* actor_group_ptr = nullptr;
  ActorSystem* actor_system_ptr = nullptr;
};

template<typename ... ArgT>
decltype(auto) Requester::send(ArgT&& ... args) {
  return self.request(std::forward<ArgT>(args) ...);
}

template<typename ... ArgT>
decltype(auto) Requester::request(ArgT&& ... args) {
  return self.request(std::forward<ArgT>(args) ...);
}
} // namespace zaf

#include "actor_behavior.tpp"
