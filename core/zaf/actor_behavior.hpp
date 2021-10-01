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
  inline void send(const Actor& receiver, size_t code, Message::Type type, ArgT&& ... args) {
    if (!receiver) {
      return; // ignore message sent to null actor
    }
    receiver.visit(overloaded {
      [&](const LocalActorHandle& r) {
        auto message = make_message(LocalActorHandle{this->get_actor_id()}, code, std::forward<ArgT>(args)...);
        message->set_type(type);
        this->send(r, message);
      },
      [&](const RemoteActorHandle& r) {
        if constexpr (traits::all_serializable<ArgT ...>::value) {
          std::vector<char> bytes;
          Serializer(bytes)
            .write(this->get_actor_id())
            .write(r.remote_actor_id)
            .write(code)
            .write(type)
            .write(hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...))
            .write(std::forward<ArgT>(args) ...);
          // Have to copy the bytes here because we do not know how many bytes
          // are needed for serialization and we need to reserve more than necessary.
          this->send(r.net_sender_info->id, DefaultCodes::ForwardMessage, Message::Type::Normal,
            zmq::message_t{&bytes.front(), bytes.size()});
        } else {
          throw ZAFException("Attempt to serialize non-serializable data");
        }
      }
    });
  }

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
  inline bool receive_once(Callback&& callback, long timeout = -1) {
    if (!waiting_for_response && !pending_messages.empty()) {
      callback(pending_messages.front());
      pending_messages.pop_front();
      return true;
    }
    zmq::pollitem_t item[1] = {
      {recv_socket.handle(), 0, ZMQ_POLLIN, 0}
    };
    try {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
      int npoll = zmq::poll(item, 1, timeout);
#pragma GCC diagnostic pop
      if (npoll == 0) {
        return false;
      }
      zmq::message_t message_ptr;
      // receive current sender routing id
      if (!recv_socket.recv(message_ptr)) {
        throw ZAFException("Expect to receive a message but actually receive nothing.");
      }
      if (!recv_socket.recv(message_ptr)) {
        throw ZAFException(
          "Failed to receive a message after receiving an routing id at ", __PRETTY_FUNCTION__
        );
      }
      callback(*reinterpret_cast<Message**>(message_ptr.data()));
      return true;
    } catch (...) {
      std::throw_with_nested(ZAFException(
        "Exception caught in ", __PRETTY_FUNCTION__, " in actor ", this->actor_id
      ));
    }
  }

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
