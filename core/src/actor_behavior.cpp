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

ActorIdType ActorBehavior::get_current_sender_actor_id() const {
  if (current_sender_routing_id.empty()) {
    return 0;
  }
  // current_sender_routing_id.data() has more content than an actor id,
  // but actor id is at the head of current_sender_routing_id.data()
  // so we directly cast it to an actor id.
  auto sender_actor_id = *reinterpret_cast<ActorIdType*>(
    const_cast<void*>(current_sender_routing_id.data())
  );
  return sender_actor_id;
}

Actor ActorBehavior::get_current_sender_actor() const {
  return {get_current_sender_actor_id()};
}

Actor ActorBehavior::get_self_actor() {
  return {this->actor_id};
}

void ActorBehavior::activate() {
  this->activated = true;
}

void ActorBehavior::deactivate() {
  this->activated = false;
}

bool ActorBehavior::is_activated() const {
  return this->activated;
}

bool ActorBehavior::receive_once(MessageHandlers&& handlers, bool non_blocking) {
  return this->receive_once(handlers, non_blocking);
}

bool ActorBehavior::receive_once(MessageHandlers& handlers, bool non_blocking) {
  try {
    if (!recv_socket.recv(current_sender_routing_id,
        non_blocking ? zmq::recv_flags::dontwait : zmq::recv_flags::none)) {
      return false;
    }
    zmq::message_t message_ptr;
    if (!recv_socket.recv(message_ptr)) {
      LOG(ERROR) << "Failed to receive a message after receiving an routing id.";
      return false;
    }
    Message* message = *reinterpret_cast<Message**>(message_ptr.data());
    handlers.process(*message);
    delete message;
    return true;
  } catch (...) {
    LOG(ERROR) << "Exception caught in " << __PRETTY_FUNCTION__ << " in actor " << this->actor_id;
    throw;
  }
}

void ActorBehavior::receive(MessageHandlers&& handlers, bool non_blocking) {
  this->receive(handlers, non_blocking);
}

void ActorBehavior::receive(MessageHandlers& handlers, bool non_blocking) {
  while (this->activated) {
    this->receive_once(handlers, non_blocking);
  }
}

void ActorBehavior::initialize_actor(ActorSystem& sys) {
  try {
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
    auto send_routing_id = get_routing_id(actor_id, true);
    send_socket.set(zmq::sockopt::routing_id, zmq::buffer(send_routing_id));
    send_socket.set(zmq::sockopt::sndhwm, 0);
    send_socket.connect("inproc://" + recv_routing_id);
    connected_receivers.insert(this->get_actor_id());
  } catch (...) {
    LOG(ERROR) << "Exception caught in " << __PRETTY_FUNCTION__ << " in actor " << this->actor_id;
    throw;
  }
}

zmq::socket_t& ActorBehavior::get_recv_socket() {
  return recv_socket;
}

void ActorBehavior::launch() {
  start();
  activate();
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

void ActorBehavior::connect_to(ActorIdType peer_id) {
  auto routing_id = "inproc://" + get_routing_id(peer_id, false);
  try {
    send_socket.connect(routing_id);
  } catch (...) {
    LOG(ERROR) << this->get_actor_id() << " failed to connect to receiver " << peer_id << " via " << routing_id;
    throw;
  }
}
} // namespace zaf
