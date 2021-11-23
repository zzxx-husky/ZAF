#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <utility>

#include "actor.hpp"
#include "count_pointer.hpp"
#include "delayed_message.hpp"
#include "make_message.hpp"
#include "message_handlers.hpp"
#include "zaf_exception.hpp"

#include "zmq.hpp"

namespace zaf {
class ActorSystem;
class ActorGroup;
class ActorBehavior;

class ActorBehavior {
protected:
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;

public:
  ActorBehavior();

  /**
   * To be overrided by subclass
   **/
  virtual void start();
  virtual void stop();
  virtual MessageHandlers behavior();

  // send a normal message to message sender
  template<typename ... ArgT>
  inline void reply(Code code, ArgT&& ... args) {
    if (this->get_current_message().get_body().get_code() == DefaultCodes::Request) {
      this->send(this->get_current_sender_actor(), DefaultCodes::Response,
        std::unique_ptr<MessageBody>(
          new_message(code, std::forward<ArgT>(args) ...)
        ));
    } else {
      this->send(this->get_current_sender_actor(), code, std::forward<ArgT>(args)...);
    }
  }

  // reply a message to the sender of the `msg`
  template<typename ... ArgT>
  inline void reply(Message& msg, Code code, ArgT&& ... args) {
    if (msg.get_body().get_code() == DefaultCodes::Request) {
      this->send(msg.get_sender(),
        DefaultCodes::Response, std::unique_ptr<MessageBody>(
          new_message(code, std::forward<ArgT>(args) ...)
        ));
    } else {
      this->send(msg.get_sender(), code, std::forward<ArgT>(args)...);
    }
  }

  // send a message to a ActorBehavior
  template<typename ... ArgT>
  inline void send(ActorBehavior& receiver, Code code, ArgT&& ... args) {
    this->send(receiver.get_local_actor_handle(), code, std::forward<ArgT>(args)...);
  }

  // send a message to Actor
  template<typename ... ArgT>
  void send(const Actor& receiver, Code code, ArgT&& ... args);

  // send a message to LocalActorHandle
  template<typename ... ArgT>
  void send(const LocalActorHandle& receiver, Code code, ArgT&& ... args) {
    auto m = new_message(Actor{this->get_local_actor_handle()},
      code, std::forward<ArgT>(args)...);
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

  // the API is similar with receive_once
  // the difference is that inner_receive_once does not handle delayed messages
  template<typename Callback,
    typename std::enable_if_t<std::is_invocable_v<Callback, Message*>>* = nullptr>
  bool inner_receive_once(Callback&& callback, long timeout = -1);

  template<typename Rep, typename Period, typename ... ArgT>
  void delayed_send(const std::chrono::duration<Rep, Period>& delay, ActorBehavior& receiver,
    Code code, ArgT&& ... args) {
    this->delayed_send(delay, Actor{receiver.get_local_actor_handle()},
      code, std::forward<ArgT>(args) ...);
  }

  template<typename Rep, typename Period, typename ... ArgT>
  void delayed_send(const std::chrono::duration<Rep, Period>& delay, const Actor& receiver,
    Code code, ArgT&& ... args);

  struct RequestHandler {
    ActorBehavior& self;
    void on_reply(MessageHandlers&& handlers);
    void on_reply(MessageHandlers& handlers);
  };

  template<typename Receiver, typename ... ArgT>
  inline RequestHandler request(Receiver&& receiver, Code code, ArgT&& ... args) {
    this->send(receiver, DefaultCodes::Request, std::unique_ptr<MessageBody>(
      new_message(code, std::forward<ArgT>(args) ...)
    ));
    return {*this};
  }

  ActorSystem& get_actor_system();
  ActorGroup& get_actor_group();
  Actor get_self_actor();
  Actor get_current_sender_actor() const;

  CountPointer<Message> take_current_message();
  const Message& get_current_message() const;

  void activate();
  void deactivate();
  bool is_activated() const;

  virtual std::string get_name() const;

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

  virtual LocalActorHandle get_local_actor_handle() const;

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

  // to handle recursive request/response
  unsigned waiting_for_response = 0;
  std::deque<Message*> pending_messages;

  ActorIdType actor_id{~0u};
  zmq::socket_t send_socket, recv_socket;
  std::vector<zmq::pollitem_t> recv_poll_items;
  ActorGroup* actor_group_ptr = nullptr;
  ActorSystem* actor_system_ptr = nullptr;

  MessageHandlers inner_handlers{}; // default: empty handlers
};
} // namespace zaf

#include "actor_behavior.tpp"
#include "requester.hpp"
