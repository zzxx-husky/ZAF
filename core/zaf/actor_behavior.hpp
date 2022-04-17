#pragma once

#include <cstdint>
#include <deque>
#include <string>
#include <utility>

#include "actor.hpp"
#include "count_pointer.hpp"
#include "delayed_message.hpp"
#include "macros.hpp"
#include "make_message.hpp"
#include "message_handlers.hpp"
#include "receive_guard.hpp"
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
    this->reply(this->get_current_message(), code, std::forward<ArgT>(args) ...);
  }

  // reply a message to the sender of the `msg`
  template<typename ... ArgT>
  void reply(const Message& msg, Code code, ArgT&& ... args);

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
  // in case there are multiple recv_polls, receive once from each of the recv_polls that have messages
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

  ActorSystem& get_actor_system();
  ActorGroup& get_actor_group();
  Actor get_self_actor();
  Actor get_current_sender_actor() const;

  CountPointer<Message> take_current_message();
  Message& get_current_message();
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

  // allow operator to register zmq::socket such that these sockets are polled together
  // call the function if the socket has incoming msgs
  virtual void add_recv_poll(zmq::socket_t& socket, std::function<void()> callback);
  // call the function when the socket is removed
  // socket should be kept alive until the callback is called.
  virtual void remove_recv_poll(zmq::socket_t& socket, std::function<void()> callback);

  virtual void launch();

  virtual ~ActorBehavior();

  ActorIdType get_actor_id() const;

  virtual LocalActorHandle get_local_actor_handle() const;

protected:
  void connect(ActorIdType peer);
  void disconnect(ActorIdType peer);

  std::optional<std::chrono::milliseconds> remaining_time_to_next_delayed_message() const;
  void flush_delayed_messages();

  void process_recv_poll_reqs();

  // time to send -> delayed message
  DefaultSortedMultiMap<TimePoint, DelayedMessage> delayed_messages;

  std::string routing_id_buffer;
  const std::string& get_routing_id(ActorIdType id, bool send_or_recv);

  DefaultHashSet<ActorIdType> connected_receivers;
  bool activated = false;
  Message* current_message = nullptr;

  ActorIdType actor_id{~0u};
  zmq::socket_t send_socket, recv_socket;
  ActorGroup* actor_group_ptr = nullptr;
  ActorSystem* actor_system_ptr = nullptr;

  std::vector<zmq::pollitem_t> recv_poll_items;
  std::vector<std::function<void()>> recv_poll_callbacks;
  std::vector<std::tuple<bool, zmq::socket_t*, std::function<void()>>> recv_poll_reqs;

  MessageHandlers inner_handlers{}; // default: empty handlers

public:
  class RequestHandler {
  private:
    ActorBehavior* self = nullptr;
    unsigned request_id;

  public:
    RequestHandler(ActorBehavior& self, unsigned req_id);
    RequestHandler(const RequestHandler&) = delete;
    RequestHandler(RequestHandler&&);
    ~RequestHandler();

    RequestHandler& operator=(const RequestHandler&) = delete;
    RequestHandler& operator=(RequestHandler&&);

    void on_reply(MessageHandlers&& handlers);
    void on_reply(MessageHandlers& handlers);
  };

  template<typename Receiver, typename ... ArgT>
  inline RequestHandler request(Receiver&& receiver, Code code, ArgT&& ... args) {
    auto req_id = request_id++;
    this->send(receiver, DefaultCodes::Request, req_id,
      std::unique_ptr<MessageBody>(
        new_message(code, std::forward<ArgT>(args) ...)
      ));
    return {*this, req_id};
  }

  void store_response(unsigned req_id, std::unique_ptr<MessageBody>& response);

protected:
  unsigned waiting_for_response = 0;
  unsigned request_id = 0;
  DefaultHashMap<unsigned, std::unique_ptr<MessageBody>> unfinished_requests;
  std::deque<Message*> pending_messages;
};
} // namespace zaf

#include "requester.hpp"
#include "actor_behavior.tpp"
