#include "zaf/actor_behavior.hpp"
#include "zaf/actor_system.hpp"
#include "zaf/zaf_exception.hpp"

namespace zaf {
ActorBehavior::DelayedMessage::DelayedMessage(const Actor& r, Message* m):
  receiver(r),
  message(m) {
}

ActorBehavior::DelayedMessage::DelayedMessage(const Actor& r, zmq::message_t&& m):
  receiver(r),
  message(std::move(m)) {
}

void ActorBehavior::start() {}

void ActorBehavior::stop() {}

MessageHandlers ActorBehavior::behavior() { return {}; }

ActorSystem& ActorBehavior::get_actor_system() {
  return *actor_system_ptr;
}

ActorGroup& ActorBehavior::get_actor_group() {
  return *actor_group_ptr;
}

Actor ActorBehavior::get_self_actor() {
  return {this->actor_id};
}

Message& ActorBehavior::get_current_message() const {
  return *this->current_message;
}

Actor ActorBehavior::get_current_sender_actor() const {
  return this->current_message
    ? this->current_message->get_sender_actor()
    : Actor{};
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

void ActorBehavior::send(const LocalActorHandle& receiver, Message* m) {
  auto receiver_id = receiver.local_actor_id;
  if (connected_receivers.insert(receiver_id).second) {
    // newly inserted
    connect(receiver_id);
  }
  auto recv_routing_id = get_routing_id(receiver_id, false);
  try {
    send_socket.send(zmq::buffer(recv_routing_id), zmq::send_flags::sndmore);
  } catch (...) {
    std::throw_with_nested(ZAFException(
      "Failed to send a multi-part zmq message for receiver routing id:\n",
      "  from sender: ", this->actor_id, '\n',
      "  to receiver: ", receiver_id, '\n',
      "  receiver routing id: ", recv_routing_id
    ));
  }
  try {
    send_socket.send(zmq::const_buffer(&m, sizeof(m)));
  } catch (...) {
    if (!bool(send_socket)) {
      std::throw_with_nested(ZAFException(
        "Exception caught when sending a message in actor ", this->actor_id, '\n',
        "  Attempt to send message before sockets are setup."
      ));
    } else {
      std::throw_with_nested(ZAFException(
        "Exception caught when sending a message in actor ", this->actor_id
      ));
    }
  }
}

bool ActorBehavior::receive_once(MessageHandlers&& handlers, bool non_blocking) {
  return this->receive_once(handlers, non_blocking);
}

bool ActorBehavior::receive_once(MessageHandlers& handlers, bool non_blocking) {
  return this->receive_once(handlers, long(non_blocking ? 0 : -1));
}

bool ActorBehavior::receive_once(MessageHandlers&& handlers, const std::chrono::milliseconds& timeout) {
  return this->receive_once(handlers, timeout);
}

bool ActorBehavior::receive_once(MessageHandlers& handlers, const std::chrono::milliseconds& timeout) {
  return this->receive_once(handlers, static_cast<long>(timeout.count()));
}

bool ActorBehavior::receive_once(MessageHandlers&& handlers, long timeout) {
  return this->receive_once(handlers, timeout);
}

bool ActorBehavior::receive_once(MessageHandlers& handlers, long timeout) {
  inner_handlers.add_child_handlers(handlers);
  return this->receive_once([&](Message* m) {
    this->current_message = m;
    try {
      inner_handlers.process(*m);
    } catch (...) {
      std::throw_with_nested(ZAFException(
        "Exception caught when processing a message with code ", m->get_code()
      ));
    }
    if (this->current_message) {
      delete this->current_message;
      this->current_message = nullptr;
    }
  }, timeout);
}

void ActorBehavior::receive(MessageHandlers&& handlers) {
  this->receive(handlers);
}

void ActorBehavior::receive(MessageHandlers& handlers) {
  this->activate();
  while (this->is_activated()) {
    this->receive_once(handlers);
  }
}

void ActorBehavior::initialize_recv_socket() {
  auto recv_routing_id = get_routing_id(this->actor_id, false);
  recv_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::router);
  // must set routing_id before bind
  recv_socket.set(zmq::sockopt::routing_id, zmq::buffer(recv_routing_id));
  recv_socket.bind("inproc://" + recv_routing_id);
}

void ActorBehavior::terminate_recv_socket() {
  auto recv_routing_id = get_routing_id(this->actor_id, false);
  recv_socket.unbind("inproc://" + recv_routing_id);
  recv_socket.close();
}

void ActorBehavior::initialize_send_socket() {
  auto send_routing_id = get_routing_id(this->actor_id, true);
  send_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::router);
  // must set routing_id before connect
  send_socket.set(zmq::sockopt::routing_id, zmq::buffer(send_routing_id));
  send_socket.set(zmq::sockopt::sndhwm, 0);
  // connect to self
  connected_receivers.insert(this->get_actor_id());
  connect(this->actor_id);
}

void ActorBehavior::terminate_send_socket() {
  for (auto& i : connected_receivers) {
    disconnect(i);
  }
  send_socket.close();
}

void ActorBehavior::initialize_actor(ActorSystem& sys, ActorGroup& group) {
  if (bool(actor_system_ptr)) {
    throw ZAFException("Actor is initialized twice. Existing actor id: ", this->actor_id);
  }
  try {
    actor_system_ptr = &sys;
    actor_group_ptr = &group;
    sys.inc_num_alive_actors();
    this->actor_id = sys.get_next_available_actor_id();
    this->initialize_routing_id_buffer();
    this->initialize_recv_socket();
    this->initialize_send_socket();
  } catch (...) {
    std::throw_with_nested(ZAFException(
      "Exception caught in ", __PRETTY_FUNCTION__, " in actor ", this->actor_id
    ));
  }
}

void ActorBehavior::initialize_routing_id_buffer() {
  routing_id_buffer.clear();
  routing_id_buffer.reserve(ActorIdMaxLen + this->get_actor_system().get_identifier().size() + 1);
  routing_id_buffer.append(ActorIdMaxLen, '0');
  routing_id_buffer.append(this->get_actor_system().get_identifier());
  routing_id_buffer.push_back(' ');
}

zmq::socket_t& ActorBehavior::get_recv_socket() {
  return recv_socket;
}

zmq::socket_t& ActorBehavior::get_send_socket() {
  return send_socket;
}

void ActorBehavior::launch() {
  this->activate();
  this->start();
  if (this->is_activated()) {
    this->receive(this->behavior());
  }
  this->stop();
}

void ActorBehavior::terminate_actor() {
  if (!actor_system_ptr) {
    return;
  }
  terminate_send_socket();
  terminate_recv_socket();
  actor_system_ptr->dec_num_alive_actors();
  actor_system_ptr = nullptr;
}

ActorBehavior::~ActorBehavior() {
  this->terminate_actor();
}

ActorIdType ActorBehavior::get_actor_id() const {
  return actor_id;
}

void ActorBehavior::connect(ActorIdType peer_id) {
  auto routing_id = "inproc://" + get_routing_id(peer_id, false);
  try {
    send_socket.connect(routing_id);
  } catch (...) {
    std::throw_with_nested(ZAFException(
      "Actor", this->get_actor_id(), " failed to connect to receiver actor ", peer_id, " via ", routing_id
    ));
  }
}

void ActorBehavior::disconnect(ActorIdType peer_id) {
  auto routing_id = "inproc://" + get_routing_id(peer_id, false);
  try {
    send_socket.disconnect(routing_id);
  } catch (...) {
    std::throw_with_nested(ZAFException(
      this->get_actor_id(), " failed to disconnect to receiver ", peer_id, " via ", routing_id
    ));
  }
}

// [actor id][zaf][s/r] as the routing id of the zmq socket.
// Note: originally we used the exact bytes of the actor id in the routing_id, however,
// zmq_connect requires a `const char*` instead of `std::string`, in case the routing id
// contains '\0', zmq ignores the part after the first '\0'. Sad.
const std::string& ActorBehavior::get_routing_id(ActorIdType id, bool send_or_recv) {
  for (int i = 0; i < ActorIdMaxLen; i++) {
    routing_id_buffer[i] = id % 10 + '0';
    id /= 10;
  }
  routing_id_buffer.back() = send_or_recv ? 's' : 'r';
  return routing_id_buffer;
}

void ActorBehavior::RequestHelper::on_reply(MessageHandlers&& handlers) {
  this->on_reply(handlers);
}

// Note: need to store the current status of ActorBehavior and create a new round of `receive`
void ActorBehavior::RequestHelper::on_reply(MessageHandlers& handlers) {
  ++self.waiting_for_response;
  auto cur_act = self.is_activated();
  self.receive([&](Message* m) {
    if (m->get_type() == Message::Type::Response) {
      auto cur_msg = self.current_message;
      self.current_message = m;
      try {
        handlers.process(*m);
      } catch (...) {
        std::throw_with_nested(ZAFException(
          "Exception caught when processing a message with code ", m->get_code()
        ));
      }
      if (self.current_message) {
        delete self.current_message;
      }
      self.current_message = cur_msg;
      self.deactivate();
    } else {
      self.pending_messages.push_back(m);
    }
  });
  if (cur_act) { self.activate(); }
  --self.waiting_for_response;
}

std::optional<std::chrono::milliseconds> ActorBehavior::remaining_time_to_next_delayed_message() const {
  return delayed_messages.empty()
    ? std::nullopt
    : std::optional(std::chrono::duration_cast<std::chrono::milliseconds>(delayed_messages.begin()->first - std::chrono::steady_clock::now()));
}

void ActorBehavior::flush_delayed_messages() {
  while (!delayed_messages.empty() &&
         delayed_messages.begin()->first <= std::chrono::steady_clock::now()) {
    auto& msg = delayed_messages.begin()->second;
    std::visit(overloaded{
      [&](Message* m) {
        LocalActorHandle& r = msg.receiver;
        this->send(r, m);
      },
      [&](zmq::message_t& m) {
        RemoteActorHandle& r = msg.receiver;
        this->send(LocalActorHandle{r.net_sender_info->id}, DefaultCodes::ForwardMessage, std::move(m));
      }
    }, msg.message);
    delayed_messages.erase(delayed_messages.begin());
  }
}
} // namespace zaf
