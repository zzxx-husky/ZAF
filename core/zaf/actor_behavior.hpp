#pragma once

#include <cstdint>

#include "actor.hpp"
#include "message_handler.hpp"

#include "zmq.hpp"

namespace zaf {
class ActorSystem;
/**
 * 1. We use [actor id][zaf][s/r] as the routing id of the zmq socket.
 **/
class ActorBehavior {
public:
  ActorBehavior();

  /**
   * To be overrided by subclass
   **/
  virtual void start();
  virtual void stop();
  virtual MessageHandlers behavior();

  /**
   * To be used by subclass
   **/
  template<typename ... ArgT>
  inline void reply(size_t code, ArgT&& ... args) {
    this->send(this->get_current_sender_actor_id(), code, std::forward<ArgT>(args)...);
  }

  template<typename ... ArgT>
  inline void send(const Actor& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver.get_actor_id(), code, std::forward<ArgT>(args)...);
  }

  template<typename ... ArgT>
  inline void send(ActorBehavior& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver.get_actor_id(), code, std::forward<ArgT>(args)...);
  }

  template<typename ... ArgT>
  void send(ActorIdType receiver_id, size_t code, ArgT&& ... args) {
    if (connected_receivers.insert(receiver_id).second) {
      // newly inserted
      connect_to(receiver_id);
    }
    auto recv_routing_id = get_routing_id(receiver_id, false);
    try {
      send_socket.send(zmq::buffer(recv_routing_id), zmq::send_flags::sndmore);
    } catch (...) {
      LOG(ERROR) << "Failed to send a multi-part zmq message for receiver routing id:\n"
        << "  from sender: " << this->actor_id << '\n'
        << "  to receiver: " << receiver_id << '\n'
        << "  receiver routing id: " << recv_routing_id;
      throw;
    }
    try {
      // the Message*
      Message* message = make_message(code, std::forward<ArgT>(args)...);
      send_socket.send(zmq::const_buffer(&message, sizeof(message)));
    } catch (...) {
      LOG(ERROR) << "Exception caught when sending a message in actor " << this->actor_id;
      LOG_IF(ERROR, !bool(send_socket)) <<
        "Attempt to send message before sockets are setup.";
      throw;
    }
  }

  // to process one incoming message with message handlers
  bool receive_once(MessageHandlers&& handlers, bool non_blocking = false);
  bool receive_once(MessageHandlers& handlers, bool non_blocking = false);
  void receive(MessageHandlers&& handlers, bool non_blocking = false);
  void receive(MessageHandlers& handlers, bool non_blocking = false);

  ActorSystem& get_actor_system();

  ActorIdType get_current_sender_actor_id() const;

  Actor get_current_sender_actor() const;

  Actor get_self_actor();

  void activate();
  void deactivate();
  bool is_activated() const;

  /**
   * To be used by ZAF
   **/
  void initialize_actor(ActorSystem& sys);

  zmq::socket_t& get_recv_socket();

  virtual void launch();

  virtual ~ActorBehavior();

  ActorIdType get_actor_id() const;

private:
  void connect_to(ActorIdType peer);

  const std::string& get_routing_id(ActorIdType id, bool send_or_recv) {
    *reinterpret_cast<ActorIdType*>(&routing_id_buffer.front()) = id;
    routing_id_buffer.back() = send_or_recv ? 's' : 'r';
    return routing_id_buffer;
  }

  ActorIdType actor_id;

  std::string routing_id_buffer;

  ActorSystem* actor_system_ptr = nullptr;
  zmq::socket_t send_socket, recv_socket;
  zmq::message_t current_sender_routing_id;
  bool activated = false;
  DefaultHashSet<ActorIdType> connected_receivers;
};
} // namespace zaf
