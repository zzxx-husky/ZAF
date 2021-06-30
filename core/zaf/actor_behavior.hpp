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
  void send(const Actor& receiver, size_t code, ArgT&& ... args) {
    if (connected_receivers.insert(receiver).second) {
      // newly inserted
      connect_to(receiver);
    }
    // the ActorBehavior* of receiver
    auto recv_routing_id = get_routing_id(receiver.get_actor_id(), false);
    send_socket.send(zmq::buffer(recv_routing_id), zmq::send_flags::sndmore);
    // the Message*
    Message* message = make_message(code, std::forward<ArgT>(args)...);
    send_socket.send(zmq::const_buffer(&message, sizeof(message)));
  }

  // to process one incoming message with message handlers
  void receive(MessageHandlers&& handlers);
  void receive(MessageHandlers& handlers);

  ActorSystem& get_actor_system();

  Actor get_current_sender_actor();

  Actor get_self_actor();

  void deactivate();

  /**
   * To be used by ZAF
   **/
  void initialize_actor(ActorSystem& sys);

  void launch();

  virtual ~ActorBehavior();

  ActorIdType get_actor_id() const;

private:
  void connect_to(const Actor& peer);

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
  DefaultHashSet<Actor> connected_receivers;
};
} // namespace zaf
