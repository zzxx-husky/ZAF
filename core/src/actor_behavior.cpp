#include <chrono>
#include <memory>
#include <string>

#include "zaf/actor.hpp"
#include "zaf/actor_behavior.hpp"
#include "zaf/actor_system.hpp"
#include "zaf/count_pointer.hpp"
#include "zaf/receive_guard.hpp"
#include "zaf/thread_utils.hpp"
#include "zaf/zaf_exception.hpp"

#include "zmq.hpp"

namespace zaf {
ActorBehavior::ActorBehavior() {
  inner_handlers.add_handlers(
    DefaultCodes::Request -
    [&](unsigned /*req_id*/, std::unique_ptr<MessageBody>& request) {
      // req_id is ignored because later it can be extracted via get_request_id()
      inner_handlers.process_body(*request);
    },
    DefaultCodes::Response -
    [&](unsigned req_id, std::unique_ptr<MessageBody>& response) {
      store_response(req_id, response);
    }
  );
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
  return Actor{get_local_actor_handle()};
}

CountPointer<Message> ActorBehavior::take_current_message() {
  if (!this->current_message) {
    throw ZAFException("Attempt to take null message.");
  }
  CountPointer<Message> ptr{this->current_message};
  this->current_message = nullptr;
  return ptr;
}

Message& ActorBehavior::get_current_message() {
  if (!this->current_message) {
    throw ZAFException("Attempt to get null message.");
  }
  return *this->current_message;
}

const Message& ActorBehavior::get_current_message() const {
  if (!this->current_message) {
    throw ZAFException("Attempt to get null message.");
  }
  return *this->current_message;
}

Actor ActorBehavior::get_current_sender_actor() const {
  return this->current_message
    ? this->current_message->get_sender()
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

std::string ActorBehavior::get_name() const {
  return to_string("ZAF/A", this->get_actor_id());
}

void ActorBehavior::send(const LocalActorHandle& receiver, Message* m) {
  auto receiver_id = receiver.local_actor_id;
  if (connected_receivers.insert(receiver_id).second) {
    // newly inserted
    connect(receiver_id);
  }
  auto recv_routing_id = get_routing_id(receiver_id, false);
  try {
    receive_guard([&]() {
      send_socket.send(zmq::buffer(recv_routing_id), zmq::send_flags::sndmore);
    });
  } catch (...) {
    std::throw_with_nested(ZAFException(
      "Failed to send a multi-part zmq message for receiver routing id:\n",
      "  from sender: ", this->actor_id, '\n',
      "  to receiver: ", receiver_id, '\n',
      "  receiver routing id: ", recv_routing_id));
  }
  try {
    receive_guard([&]() {
      send_socket.send(zmq::const_buffer(&m, sizeof(m)));
    });
  } catch (...) {
    if (!bool(send_socket)) {
      std::throw_with_nested(ZAFException(
        "Exception caught when sending a message in actor ", this->actor_id, '\n',
        "  Attempt to send message before sockets are setup."));
    } else {
      std::throw_with_nested(ZAFException(
        "Exception caught when sending a message in actor ", this->actor_id));
    }
  }
}

bool ActorBehavior::receive_once(MessageHandlers&& handlers, bool non_blocking) {
  return this->receive_once(handlers, non_blocking);
}

bool ActorBehavior::receive_once(MessageHandlers& handlers, bool non_blocking) {
  return this->receive_once(handlers, long(non_blocking ? 0 : -1));
}

bool ActorBehavior::receive_once(MessageHandlers&& handlers,
  const std::chrono::milliseconds& timeout) {
  return this->receive_once(handlers, timeout);
}

bool ActorBehavior::receive_once(MessageHandlers& handlers,
  const std::chrono::milliseconds& timeout) {
  return this->receive_once(handlers, static_cast<long>(timeout.count()));
}

bool ActorBehavior::receive_once(MessageHandlers&& handlers, long timeout) {
  return this->receive_once(handlers, timeout);
}

bool ActorBehavior::receive_once(MessageHandlers& handlers, long timeout) {
  return this->receive_once([&](Message* m) {
    auto prev_message = this->current_message;
    this->current_message = m;
    inner_handlers.add_child_handlers(handlers);
    try {
      inner_handlers.process(*m);
    } catch (...) {
      std::throw_with_nested(ZAFException(
        "Exception caught when processing a message with code ", m->get_body().get_code(),
        " (", std::hex, m->get_body().get_code(), ")."));
    }
    inner_handlers.remove_child_handlers();
    if (this->current_message) {
      delete this->current_message;
    }
    this->current_message = prev_message;
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
  recv_socket = zmq::socket_t(
    this->get_actor_system().get_zmq_context(), zmq::socket_type::router);
  // must set routing_id before bind
  recv_socket.set(zmq::sockopt::routing_id, zmq::buffer(recv_routing_id));
  recv_socket.bind("inproc://" + recv_routing_id);
  recv_poll_items.emplace_back(zmq::pollitem_t{
    recv_socket.handle(), 0, ZMQ_POLLIN, 0
  });
  recv_poll_callbacks.push_back(nullptr);
}

void ActorBehavior::terminate_recv_socket() {
  auto recv_routing_id = get_routing_id(this->actor_id, false);
  recv_socket.unbind("inproc://" + recv_routing_id);
  recv_socket.close();
  recv_poll_items.clear();
}

void ActorBehavior::initialize_send_socket() {
  auto send_routing_id = get_routing_id(this->actor_id, true);
  send_socket = zmq::socket_t(
    this->get_actor_system().get_zmq_context(), zmq::socket_type::router);
  // must set routing_id before connect
  send_socket.set(zmq::sockopt::routing_id, zmq::buffer(send_routing_id));
  send_socket.set(zmq::sockopt::sndhwm, 0);
  // send_socket.set(zmq::sockopt::linger, 0);
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
  actor_system_ptr = &sys;
  actor_group_ptr = &group;
  sys.inc_num_alive_actors();
  this->actor_id = sys.get_next_available_actor_id();
  this->initialize_routing_id_buffer();
  try {
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
  routing_id_buffer.reserve(ActorIdMaxLen +
    this->get_actor_system().get_identifier().size() + 1);
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

void ActorBehavior::add_recv_poll(
  zmq::socket_t& socket, std::function<void()> callback) {
  if (callback == nullptr) {
    throw ZAFException("Callback for newly inserted recv socket must not be null.");
  }
  recv_poll_reqs.emplace_back(true, &socket, std::move(callback));
}

void ActorBehavior::remove_recv_poll(
  zmq::socket_t& socket, std::function<void()> callback) {
  recv_poll_reqs.emplace_back(false, &socket, std::move(callback));
}

void ActorBehavior::process_recv_poll_reqs() {
  if (!recv_poll_reqs.empty()) {
    for (auto& i : recv_poll_reqs) {
      auto&& [add_or_del, socket, callback] = i;
      auto&& handle = socket->handle();
      if (add_or_del) {
        recv_poll_items.emplace_back(zmq::pollitem_t{
          handle, 0, ZMQ_POLLIN, 0
        });
        recv_poll_callbacks.push_back(std::move(callback));
      } else {
        bool flag_found = false;
        for (int j = 1, n = recv_poll_items.size(); j < n; j++) {
          if (recv_poll_items[j].socket == handle) {
            if (callback) {
              callback();
            }
            if (j != n - 1) {
              std::swap(recv_poll_items[j], recv_poll_items.back());
              std::swap(recv_poll_callbacks[j], recv_poll_callbacks.back());
            }
            recv_poll_items.pop_back();
            recv_poll_callbacks.pop_back();
            flag_found = true;
            break;
          }
        }
        if (!flag_found) {
          throw ZAFException(to_string(
            "Failed to find socket with handle ", handle,
            ". Do you keep the socket alive before it is removed?"));
        }
      }
    }
    recv_poll_reqs.clear();
  }
}

void ActorBehavior::launch() {
  thread::set_name(this->get_name());
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
  for (auto& m : pending_messages) {
    delete m;
  }
  pending_messages.clear();
  delayed_messages.clear();
  while (true) {
    // a workaround to avoid memory leak on messages because the receiver
    // actor is terminated and messages are not processed and freed.
    if (!this->inner_receive_once([](Message* m) { delete m; }, 1)) {
      break;
    }
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

LocalActorHandle ActorBehavior::get_local_actor_handle() const {
  return {actor_id, false};
}

void ActorBehavior::connect(ActorIdType peer_id) {
  auto routing_id = "inproc://" + get_routing_id(peer_id, false);
  try {
    send_socket.connect(routing_id);
  } catch (...) {
    std::throw_with_nested(ZAFException(
      "Actor", this->get_actor_id(), " failed to connect to receiver actor ",
      peer_id, " via ", routing_id));
  }
}

void ActorBehavior::disconnect(ActorIdType peer_id) {
  auto routing_id = "inproc://" + get_routing_id(peer_id, false);
  try {
    send_socket.disconnect(routing_id);
  } catch (...) {
    std::throw_with_nested(ZAFException(
      this->get_actor_id(), " failed to disconnect to receiver ",
      peer_id, " via ", routing_id));
  }
}

// [actor id][zaf][s/r] as the routing id of the zmq socket.
// Note: originally we used the exact bytes of the actor id in the routing_id, however,
// zmq_connect requires a `const char*` instead of `std::string`, in case the routing id
// contains '\0', zmq ignores the part after the first '\0'. Sad.
const std::string& ActorBehavior::get_routing_id(ActorIdType id, bool send_or_recv) {
  for (size_t i = 0; i < ActorIdMaxLen; i++) {
    routing_id_buffer[i] = id % 10 + '0';
    id /= 10;
  }
  routing_id_buffer.back() = send_or_recv ? 's' : 'r';
  return routing_id_buffer;
}

ActorBehavior::RequestHandler::RequestHandler(ActorBehavior& self, unsigned req_id):
  self(&self),
  request_id(req_id) {
  this->self->unfinished_requests.emplace(req_id, nullptr);
}

ActorBehavior::RequestHandler::RequestHandler(RequestHandler&& other):
  self(other.self),
  request_id(other.request_id) {
  other.self = nullptr;
}

ActorBehavior::RequestHandler&
ActorBehavior::RequestHandler::operator=(ActorBehavior::RequestHandler&& other) {
  self = other.self;
  request_id = other.request_id;
  other.self = nullptr;
  return *this;
}

ActorBehavior::RequestHandler::~RequestHandler() {
  if (self) {
    self->unfinished_requests.erase(request_id);
  }
}

void ActorBehavior::RequestHandler::on_reply(MessageHandlers&& handlers) {
  this->on_reply(handlers);
}

// Note: need to store the current status of ActorBehavior and create a new round of `receive`
void ActorBehavior::RequestHandler::on_reply(MessageHandlers& handlers) {
  if (!self) {
    throw ZAFException("Attempt to call on_reply on an invalid RequestHandler.");
  }
  auto iter = self->unfinished_requests.find(request_id);
  if (iter->second) {
    // 1. if the response has been received and stored, process it
    handlers.process_body(*iter->second);
    self->unfinished_requests.erase(iter);
  } else {
    // 2. if not, wait for the response
    self->unfinished_requests.erase(iter);
    ++self->waiting_for_response;
    auto cur_act = self->is_activated();
    auto current_inner_handlers = std::move(self->inner_handlers);
    self->receive({
      DefaultCodes::Response -
      [&](unsigned req_id, std::unique_ptr<MessageBody>& rep) {
        if (req_id != request_id) {
          self->store_response(req_id, rep);
          return;
        }
        try {
          handlers.process_body(*rep);
        } catch (...) {
          std::throw_with_nested(ZAFException(
            "Exception caught when processing a message with code ", rep->get_code(),
            " (", std::hex, rep->get_code(), ")."));
        }
        self->deactivate();
      },
      DefaultCodes::DefaultMessageHandler - [&](Message&) {
        self->pending_messages.push_back(self->current_message);
        self->current_message = nullptr;
      }
    });
    self->inner_handlers = std::move(current_inner_handlers);
    if (cur_act) {
      self->activate();
    }
    --self->waiting_for_response;
  }
  self = nullptr;
}

std::optional<std::chrono::milliseconds>
ActorBehavior::remaining_time_to_next_delayed_message() const {
  return delayed_messages.empty()
    ? std::nullopt
    : std::optional(
        std::chrono::duration_cast<std::chrono::milliseconds>(
          delayed_messages.begin()->first - std::chrono::steady_clock::now()));
}

void ActorBehavior::flush_delayed_messages() {
  while (!delayed_messages.empty() &&
         delayed_messages.begin()->first <= std::chrono::steady_clock::now()) {
    auto& msg = delayed_messages.begin()->second;
    std::visit(overloaded{
      [&](Message*& m) {
        LocalActorHandle& r = static_cast<LocalActorHandle&>(msg.receiver);
        this->send(r, m);
        m = nullptr;
      },
      [&](MessageBytes& m) {
        RemoteActorHandle& r = static_cast<RemoteActorHandle&>(msg.receiver);
        this->send(r.net_sender_info->net_sender,
          DefaultCodes::ForwardMessage, std::move(m.header), std::move(m.content));
      }
    }, msg.message);
    delayed_messages.erase(delayed_messages.begin());
  }
}

void ActorBehavior::store_response(unsigned req_id,
  std::unique_ptr<MessageBody>& response) {
  // If there is a RequestHandler waiting for a response with this req_id,
  // store this response for this RequestHandler
  auto iter = unfinished_requests.find(req_id);
  if (iter != unfinished_requests.end()) {
    iter->second = std::move(response);
  } else {
    // Ignore the response.
    // Request id not found because the RequestHandler has been destroyed
    // before it processes the reply.
  }
}
} // namespace zaf
