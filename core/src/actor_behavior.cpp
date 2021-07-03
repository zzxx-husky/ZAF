#include "zaf/actor_behavior.hpp"
#include "zaf/actor_system.hpp"

namespace zaf {

ActorBehavior::ActorBehavior() {
  routing_id_buffer.reserve(sizeof(ActorIdType) + 4);
  routing_id_buffer.append(sizeof(ActorIdType), ' ');
  routing_id_buffer.append("zaf ");
}

void ActorBehavior::start() {}

void ActorBehavior::stop() {}

MessageHandlers ActorBehavior::behavior() { return {}; }

ActorSystem& ActorBehavior::get_actor_system() {
  return *actor_system_ptr;
}

Actor ActorBehavior::get_current_sender_actor() {
  if (current_sender_routing_id.empty()) {
    return {};
  }
  auto sender_actor_id = *reinterpret_cast<ActorIdType*>(current_sender_routing_id.data());
  return {sender_actor_id};
}

Actor ActorBehavior::get_self_actor() {
  return {this->actor_id};
}

void ActorBehavior::deactivate() {
  this->activated = false;
}

void ActorBehavior::receive_once(MessageHandlers&& handlers) {
  this->receive_once(handlers);
}

void ActorBehavior::receive_once(MessageHandlers& handlers) {
  if (!recv_socket.recv(current_sender_routing_id)) {
    LOG(WARNING) << "Error when receiving a routing id.";
    return;
  }
  zmq::message_t message_ptr;
  if (!recv_socket.recv(message_ptr)) {
    LOG(ERROR) << "Failed to receive a message after receiving an routing id.";
    return;
  }
  Message* message = *reinterpret_cast<Message**>(message_ptr.data());
  handlers.process(*message);
  delete message;
}

void ActorBehavior::receive(MessageHandlers&& handlers) {
  this->receive(handlers);
}

void ActorBehavior::receive(MessageHandlers& handlers) {
  this->activated = true;
  while (this->activated) {
    this->receive_once(handlers);
  }
}

void ActorBehavior::initialize_actor(ActorSystem& sys) {
  actor_system_ptr = &sys;
  sys.inc_num_alive_actors();

  auto& ctx = sys.get_zmq_context();
  recv_socket = zmq::socket_t(ctx, zmq::socket_type::router);
  send_socket = zmq::socket_t(ctx, zmq::socket_type::router);

  this->actor_id = sys.get_next_available_actor_id();
  // must set routing_id before bind
  auto recv_routing_id = get_routing_id(actor_id, false);
  recv_socket.set(zmq::sockopt::routing_id, zmq::buffer(recv_routing_id));
  recv_socket.bind("inproc://" + recv_routing_id);
  auto send_routing_id = get_routing_id(actor_id, false);
  send_socket.set(zmq::sockopt::routing_id, zmq::buffer(send_routing_id));
  send_socket.set(zmq::sockopt::sndhwm, 0);
  send_socket.connect("inproc://" + recv_routing_id);
  connected_receivers.insert(get_self_actor());
}

void ActorBehavior::launch() {
  start();
  this->receive(behavior());
  stop();
}

ActorBehavior::~ActorBehavior() {
  if (!actor_system_ptr) {
    return;
  }
  recv_socket.close();
  send_socket.close();
  actor_system_ptr->dec_num_alive_actors();
  actor_system_ptr = nullptr;
}

ActorIdType ActorBehavior::get_actor_id() const {
  return actor_id;
}

void ActorBehavior::connect_to(const Actor& peer) {
  send_socket.connect("inproc://" + get_routing_id(peer.get_actor_id(), false));
}
} // namespace zaf
