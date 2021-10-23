#include "zaf/net_gate.hpp"

namespace zaf {
NetGate::Receiver::Receiver(const std::string bind_host, const NetSenderInfo& net_sender_info):
  bind_host(bind_host),
  net_sender_info(net_sender_info) {
}

MessageHandlers NetGate::Receiver::behavior() {
  return {
    NetGate::Termination - [&]() {
      this->get_actor_system().dec_num_detached_actors();
      this->deactivate();
    },
    NetGate::BindPortReq - [&](const std::string& url) {
      // `endpoint` is in the format of `tcp://a.b.c.d:port`
      auto endpoint = net_recv_socket.get(zmq::sockopt::last_endpoint);
      auto pos = endpoint.find_last_of(':');
      auto bind_port = std::stoi(endpoint.substr(pos + 1, endpoint.size() - pos - 1));
      this->reply(BindPortRep, url, bind_port);
    }
  };
}

void NetGate::Receiver::receive_once_from_net() {
  zmq::message_t bytes;
  if (!net_recv_socket.recv(bytes)) {
    throw ZAFException(
      "Failed to receive message bytes after receiving an routing id at ", __PRETTY_FUNCTION__
    );
  }
  Deserializer s((const char*) bytes.data());
  auto send_actor = deserialize<LocalActorHandle>(s);
  auto recv_actor = deserialize<LocalActorHandle>(s);
  auto message_code = deserialize<size_t>(s);
  auto message_type = deserialize<Message::Type>(s);
  auto types_hash = deserialize<size_t>(s);
  this->send(recv_actor, new TypedSerializedMessage<zmq::message_t>(
    Actor{RemoteActorHandle{net_sender_info, send_actor}},
    message_code,
    message_type,
    types_hash,
    std::move(bytes),
    LocalActorHandle::SerializationSize + LocalActorHandle::SerializationSize + sizeof(size_t) + sizeof(Message::Type) + sizeof(size_t)
  ));
}

void NetGate::Receiver::launch() {
  std::vector<zmq::pollitem_t> poll_items{
    {net_recv_socket.handle(), 0, ZMQ_POLLIN, 0},
    {this->get_recv_socket().handle(), 0, ZMQ_POLLIN, 0}
  };
  auto msg_handlers = behavior();
  this->activate();
  while (this->is_activated()) {
    // block until receive a message
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int npoll = zmq::poll(poll_items);
#pragma GCC diagnostic pop
    if (npoll == 0) {
      continue;
    }
    if (poll_items[0].revents & ZMQ_POLLIN) {
      this->receive_once_from_net();
    }
    if (poll_items[1].revents & ZMQ_POLLIN) {
      this->receive_once(msg_handlers);
    }
  }
}

void NetGate::Receiver::initialize_recv_socket() {
  this->ActorBehaviorX::initialize_recv_socket();
  net_recv_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::pull);
  // Bind to any available port
  net_recv_socket.bind(to_string("tcp://", bind_host, ":*"));
}

void NetGate::Receiver::terminate_recv_socket() {
  this->ActorBehaviorX::terminate_recv_socket();
  // same url as the one when binding
  net_recv_socket.unbind(to_string("tcp://", bind_host, ":*"));
  net_recv_socket.close();
}

MessageHandlers NetGate::Sender::behavior() {
  return {
    NetGate::DataConnReq - [&](const std::string& host, int port) {
      this->connected_url = to_string("tcp://", host, ':', port);
      try {
        net_send_socket.connect(connected_url);
      } catch (...) {
        std::throw_with_nested(ZAFException(
          "Actor ", this->get_actor_id(), " unable to connect `", connected_url, "`."
        ));
      }
      for (auto& m : pending_messages) {
        net_send_socket.send(m, zmq::send_flags::none);
      }
      pending_messages.clear();
    },
    NetGate::Termination - [&]() {
      if (forward_any_message) {
        forward_any_message = false;
        // In case that the last alive actor sends a message to NetSender
        // and then immediately stops. Then the NetGate will be notified to terminate.
        // The termination message may go faster than the message sent by the last alive actor.
        this->delayed_send(std::chrono::milliseconds{1}, *this, NetGate::Termination);
      } else {
        this->get_actor_system().dec_num_detached_actors();
        this->deactivate();
      }
    },
    DefaultCodes::ForwardMessage - [&](zmq::message_t& msg_bytes) {
      forward_any_message = true;
      if (this->connected_url.empty()) {
        pending_messages.emplace_back(std::move(msg_bytes));
      } else {
        net_send_socket.send(msg_bytes, zmq::send_flags::none);
      }
    }
  };
}

void NetGate::Sender::initialize_send_socket() {
  this->ActorBehaviorX::initialize_send_socket();
  net_send_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::push);
  net_send_socket.set(zmq::sockopt::sndhwm, 0);
  // net_send_socket.set(zmq::sockopt::linger, 0);
}

void NetGate::Sender::terminate_send_socket() {
  if (!connected_url.empty()) {
    net_send_socket.disconnect(connected_url);
  }
  net_send_socket.close();
  this->ActorBehaviorX::terminate_send_socket();
}

NetGate::NetGateActor::NetGateActor(const std::string& host, int port):
  bind_host(host),
  bind_port(port) {
}

MessageHandlers NetGate::NetGateActor::behavior() {
  return {
    // instruction from local NetGate
    NetGate::NetGateConnReq - [&](const std::string& host, const int port) {
      this->establish_connection(to_string(host, ':', port));
    },
    NetGate::DataConnReq - [&](const std::string& connect_host, int connect_port) {
      std::string net_gate_url{(char*) current_net_gate_routing_id.data(), current_net_gate_routing_id.size() - 2};
      auto& s = [&]() -> auto& {
        try {
          return net_gate_connections.at(net_gate_url).sender;
        } catch (...) {
          std::throw_with_nested(ZAFException(
            "Non-connected net gate requests a data connection: ", net_gate_url
          ));
        }
      }();
      this->send(s, NetGate::DataConnReq, connect_host, connect_port);
    },
    // tell the peer the port that is used by the recv socket for this `url`
    NetGate::BindPortRep - [&](const std::string& url, int port) {
      this->send_to_net_gate(url, NetGate::DataConnReq, this->bind_host, port);
    },
    NetGate::ActorRegistration - [&](const std::string& name, const Actor& actor) {
      actor.visit(overloaded {
        [&](const LocalActorHandle& l) {
          auto& local_actor = local_registered_actors[name];
          local_actor.actor = l;
          if (!local_actor.requesters.empty()) {
            for (auto& i : local_actor.requesters) {
              this->send_to_net_gate(i, NetGate::RemoteActorLookupRep, name, l);
            }
            local_actor.requesters.clear();
          }
        },
        [&](const RemoteActorHandle& r) {
          throw ZAFException("Failed to register actor with name ", name, " because it is not a local actor.");
        }
      });
      
    },
    // `url` in the format of "a.b.c.d:port"
    NetGate::ActorLookupReq - [&](const std::string& url, const std::string& name) {
      this->establish_connection(url);
      auto& requesters = net_gate_connections.at(url).actor_lookup_requesters[name];
      if (requesters.empty()) {
        // no other else is asking for the same actor
        this->send_to_net_gate(url, RemoteActorLookupReq, name);
      }
      requesters.push_back(this->get_current_sender_actor());
    },
    NetGate::RemoteActorLookupReq - [&](const std::string& name) {
      std::string peer{(char*) current_net_gate_routing_id.data(), current_net_gate_routing_id.size() - 2};
      auto& local_actor = local_registered_actors[name];
      if (local_actor.actor) {
        this->send_to_net_gate(peer,
          NetGate::RemoteActorLookupRep, name, *local_actor.actor);
      } else {
        local_actor.requesters.push_back(std::move(peer));
      }
    },
    NetGate::RemoteActorLookupRep - [&](const std::string& name, const LocalActorHandle& remote_actor) {
      std::string peer{(char*) current_net_gate_routing_id.data(), current_net_gate_routing_id.size() - 2};
      auto& conn = net_gate_connections.at(peer);
      auto& requesters = conn.actor_lookup_requesters;
      auto iter = requesters.find(name);
      Actor actor{RemoteActorHandle{conn.net_sender_info, remote_actor}};
      for (auto& i : iter->second) {
        this->send(i, ActorLookupRep, peer, name, actor);
      }
      requesters.erase(iter);
    },
    NetGate::Ping - [&]() {
      std::string peer{(char*) current_net_gate_routing_id.data(), current_net_gate_routing_id.size() - 2};
      this->establish_connection(peer);
      this->imme_send_to_net_gate(peer, NetGate::Pong);
    },
    NetGate::Pong - [&]() {
      std::string peer{(char*) current_net_gate_routing_id.data(), current_net_gate_routing_id.size() - 2};
      this->receive_pong(peer);
    },
    NetGate::PingRetry - [&](const std::string& url) {
      this->ping_net_gate(url);
    },
    NetGate::Termination - [&]() {
      for (auto& i : net_gate_connections) {
        this->send(i.second.sender, NetGate::Termination);
        this->send(i.second.receiver, NetGate::Termination);
      }
      this->get_actor_system().dec_num_detached_actors();
      this->deactivate();
    },
    NetGate::RetrieveActorReq - [&](ActorInfo& info) {
      if (info.net_gate_url.empty()) {
        this->reply(NetGate::RetrieveActorRep,
          std::move(info), Actor{info.actor});
      } else {
        this->establish_connection(info.net_gate_url);
        this->reply(NetGate::RetrieveActorRep,
          std::move(info), Actor{RemoteActorHandle{
            this->net_gate_connections.at(info.net_gate_url).net_sender_info,
            info.actor
          }});
      }
    }
  };
}

// handle messages from peer NetGateActor
void NetGate::NetGateActor::receive_once_from_net_gate(MessageHandlers& handlers) {
  if (!net_recv_socket.recv(current_net_gate_routing_id, zmq::recv_flags::none)) {
    return;
  }
  zmq::message_t msg_bytes;
  if (!net_recv_socket.recv(msg_bytes, zmq::recv_flags::none)) {
    throw ZAFException("Receive empty message from ", current_net_gate_routing_id);
  }
  Deserializer s((const char*) msg_bytes.data());
  auto msg_code = deserialize<size_t>(s);
  // not reading msg_type here because it only receives Message::Type::Normal messages
  auto types_hash = deserialize<size_t>(s);
  TypedSerializedMessage<zmq::message_t> message {
    Actor{},
    msg_code,
    Message::Type::Normal,
    types_hash,
    std::move(msg_bytes),
    sizeof(size_t) + sizeof(size_t)
  };
  handlers.process(message);
}

void NetGate::NetGateActor::ping_net_gate(const std::string& ng_url) {
  if (net_gate_connections.at(ng_url).is_ponged) {
    return;
  }
  this->imme_send_to_net_gate(ng_url, NetGate::Ping);
  this->delayed_send(std::chrono::milliseconds{5}, *this, NetGate::PingRetry, ng_url);
}

void NetGate::NetGateActor::receive_pong(const std::string& peer) {
  auto& conn = net_gate_connections.at(peer);
  if (conn.is_ponged) {
    return;
  }
  conn.is_ponged = true;
  for (auto& m : conn.pending_messages) {
    net_send_socket.send(zmq::buffer(peer + "/r"), zmq::send_flags::sndmore);
    net_send_socket.send(std::move(m), zmq::send_flags::none);
  }
  conn.pending_messages.clear();
}

bool NetGate::NetGateActor::establish_connection(const std::string& url) {
  auto iter_ins = net_gate_connections.emplace(url, NetGateConn{});
  if (!iter_ins.second) {
    // 0. Connected with this net gate already
    return false;
  }
  auto& conn = iter_ins.first->second;
  // 1. Connect to this new peer
  net_send_socket.connect("tcp://" + url);
  this->ping_net_gate(url);
  // 2. Create send and recv sockets for this peer
  auto& actor_sys = this->get_actor_system();
  conn.sender = actor_sys.spawn<Sender>();
  actor_sys.inc_num_detached_actors();
  conn.net_sender_info = NetSenderInfo {
    conn.sender,
    to_string(bind_host, ':', bind_port),
    url
  };
  conn.receiver = actor_sys.spawn<Receiver>(bind_host, conn.net_sender_info);
  actor_sys.inc_num_detached_actors();
  // 3. Ask the receiver which port it binds
  auto& r = iter_ins.first->second.receiver;
  this->send(r, NetGate::BindPortReq, url);
  return true;
}

void NetGate::NetGateActor::launch() {
  {
    this->get_actor_system().add_terminator([&](auto& sys) {
      sys->send(*this, NetGate::Termination);
    });
    this->get_actor_system().inc_num_detached_actors();
  }
  std::vector<zmq::pollitem_t> poll_items {
    {net_recv_socket.handle(), 0, ZMQ_POLLIN, 0},
    {this->get_recv_socket().handle(), 0, ZMQ_POLLIN, 0}
  };
  auto msg_handlers = behavior();
  this->activate();
  while (this->is_activated()) {
    // block until receive a message
    auto timeout = this->remaining_time_to_next_delayed_message();
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    int npoll = timeout
      ? zmq::poll(poll_items, *timeout)
      : zmq::poll(poll_items);
#pragma GCC diagnostic pop
    if (timeout) {
      this->flush_delayed_messages();
    }
    if (npoll == 0) {
      continue;
    }
    if (poll_items[0].revents & ZMQ_POLLIN) {
      this->receive_once_from_net_gate(msg_handlers);
    }
    if (poll_items[1].revents & ZMQ_POLLIN) {
      this->receive_once(msg_handlers);
    }
  }
}

void NetGate::NetGateActor::initialize_send_socket() {
  this->ActorBehaviorX::initialize_send_socket();
  net_send_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::router);
  auto routing_id = to_string(bind_host, ':', bind_port, "/s");
  net_send_socket.set(zmq::sockopt::routing_id, zmq::buffer(routing_id));
  net_send_socket.set(zmq::sockopt::sndhwm, 0);
  // net_send_socket.set(zmq::sockopt::linger, 0);
}

void NetGate::NetGateActor::initialize_recv_socket() {
  this->ActorBehaviorX::initialize_recv_socket();
  net_recv_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::router);
  auto routing_id = to_string(bind_host, ':', bind_port, "/r");
  net_recv_socket.set(zmq::sockopt::routing_id, zmq::buffer(routing_id));
  auto bind_url = to_string("tcp://", bind_host, ':', bind_port);
  net_recv_socket.bind(bind_url);
}

void NetGate::NetGateActor::terminate_send_socket() {
  for (auto& i : net_gate_connections) {
    net_send_socket.disconnect(i.first);
  }
  net_send_socket.close();
  this->ActorBehaviorX::terminate_send_socket();
}

void NetGate::NetGateActor::terminate_recv_socket() {
  auto bind_url = to_string("tcp://", bind_host, ':', bind_port);
  net_recv_socket.unbind(bind_url);
  net_recv_socket.close();
  this->ActorBehaviorX::terminate_recv_socket();
}

NetGate::NetGate(ActorSystem& actor_sys, const std::string& bind_host, int port) {
  initialize(actor_sys, bind_host, port);
}

void NetGate::connect(const std::string& host, int port) {
  if (!is_initialized()) {
    throw ZAFException("NetGate is used before being initialized.");
  }
  self->send(net_gate_actor, NetGate::NetGateConnReq, host, port);
}

void NetGate::initialize(ActorSystem& actor_sys, const std::string& bind_host, int bind_port) {
  if (is_initialized()) {
    throw ZAFException("Attempt to initialize an already initialized NetGate");
  }
  self = std::move(actor_sys.create_scoped_actor());
  // host and port are stored inside NetGateActor because the NetGate object may be destroyed before NetGateActor
  net_gate_actor = actor_sys.spawn<NetGateActor>(bind_host, bind_port);
}

void NetGate::terminate() {
  if (!is_initialized()) {
    return;
  }
  this->self->send(net_gate_actor, NetGate::Termination);
  this->net_gate_actor = nullptr;
  this->self = nullptr;
}

bool NetGate::is_initialized() const {
  return this->self;
}

void NetGate::register_actor(const std::string& name, const Actor& actor) {
  if (!is_initialized()) {
    throw ZAFException("NetGate is used before being initialized.");
  }
  self->send(net_gate_actor, NetGate::ActorRegistration, name, actor);
}

Actor NetGate::actor() const {
  if (!is_initialized()) {
    throw ZAFException("NetGate is used before being initialized.");
  }
  return net_gate_actor;
}
} // namespace zaf
