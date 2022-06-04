#include "zaf/net_gate.hpp"
#include "zaf/receive_guard.hpp"
#include "zaf/thread_utils.hpp"

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
    NetGate::BindPortReq - [&]() {
      // `endpoint` is in the format of `tcp://a.b.c.d:port`
      auto endpoint = net_recv_socket.get(zmq::sockopt::last_endpoint);
      auto pos = endpoint.find_last_of(':');
      auto bind_port = std::stoi(endpoint.substr(pos + 1, endpoint.size() - pos - 1));
      this->reply(BindPortRep, bind_port);
    }
  };
}

void NetGate::Receiver::receive_once_from_net() {
  zmq::message_t message;
  receive_guard([&]() {
    if (!net_recv_socket.recv(message)) {
      throw ZAFException(
        "Failed to receive message (number of messages) as expected at ",
        __PRETTY_FUNCTION__
      );
    }
  });
  unsigned num_messages = *message.data<unsigned>();
  receive_guard([&]() {
    if (!net_recv_socket.recv(message)) {
      throw ZAFException(
        "Failed to receive message (message bytes) as expected at ",
        __PRETTY_FUNCTION__
      );
    }
  });
  Deserializer s(message.data<char>());
  for (unsigned i = 0; i < num_messages; i++) {
    auto send_actor = deserialize<LocalActorHandle>(s);
    auto recv_actor = deserialize<LocalActorHandle>(s);
    auto message_code = deserialize<Code>(s);
    auto types_hash = deserialize<size_t>(s);
    auto num_bytes = deserialize<unsigned>(s);
    auto bytes = std::vector<char>(num_bytes);
    s.read_bytes(&bytes.front(), num_bytes);
    auto msg = new TypedMessage<TypedSerializedMessageBody<std::vector<char>>>(
      Actor{RemoteActorHandle{net_sender_info, send_actor}},
      message_code,
      types_hash,
      std::move(bytes)
    );
    this->send(recv_actor, msg);
  }
}

void NetGate::Receiver::launch() {
  thread::set_name(to_string("ZAF/NGR", this->get_actor_id()));
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
        push_to_buffer(m);
      }
      pending_messages.clear();
    },
    NetGate::Termination - [&]() {
      if (forward_any_message) {
        forward_any_message = !pending_messages.empty();
        // In case that the last alive actor sends a message to NetSender
        // and then immediately stops. Then the NetGate will be notified to terminate.
        // The termination message may go faster than the message sent by the last alive actor.
        this->delayed_send(std::chrono::milliseconds{1}, *this, NetGate::Termination);
      } else {
        this->get_actor_system().dec_num_detached_actors();
        this->deactivate();
      }
    },
    NetGate::FlushBuffer - [&]() {
      this->flush_byte_buffer();
    },
    DefaultCodes::ForwardMessage - [&](MessageBytes& bytes) {
      forward_any_message = true;
      if (this->connected_url.empty()) {
        pending_messages.emplace_back(std::move(bytes));
      } else {
        push_to_buffer(bytes);
      }
    }
  };
}

void NetGate::Sender::push_to_buffer(MessageBytes& bytes) {
  // A simple bufferring. Bytes will be sent in `post_swsr_consumption`.
  byte_buffer.insert(byte_buffer.end(), bytes.header.begin(), bytes.header.end());
  byte_buffer.insert(byte_buffer.end(), bytes.content.begin(), bytes.content.end());
  if (num_buffered_messages++ == 0) {
    this->ActorBehavior::send(*this, FlushBuffer);
  }
}

void NetGate::Sender::flush_byte_buffer() {
  if (num_buffered_messages != 0) {
    zmq::const_buffer num{&num_buffered_messages, sizeof(num_buffered_messages)};
    net_send_socket.send(num, zmq::send_flags::sndmore);
    zmq::const_buffer bytes{&byte_buffer.front(), byte_buffer.size()};
    net_send_socket.send(bytes, zmq::send_flags::none);
    byte_buffer.clear();
    num_buffered_messages = 0;
  }
}

void NetGate::Sender::post_swsr_consumption() {
  this->flush_byte_buffer();
}

std::string NetGate::Sender::get_name() const {
  return to_string("ZAF/NGS", this->get_actor_id());
}

void NetGate::Sender::initialize_send_socket() {
  this->ActorBehaviorX::initialize_send_socket();
  net_send_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::push);
  net_send_socket.set(zmq::sockopt::sndhwm, 0);
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
  bind_port(port),
  bind_url(to_string(bind_host, ':', bind_port)) {
}

MessageHandlers NetGate::NetGateActor::behavior() {
  return {
    // instruction from local NetGate
    NetGate::NetGateConnReq - [&](const std::string& host, const int port) {
      this->establish_connection(to_string(host, ':', port));
    },
    NetGate::NetGateBindPortReq - [&]() {
      auto endpoint = net_recv_socket.get(zmq::sockopt::last_endpoint);
      auto pos = endpoint.find_last_of(':');
      int bind_port = std::stoi(endpoint.substr(pos + 1, endpoint.size() - pos - 1));
      this->reply(NetGateBindPortRep, bind_port);
    },
    NetGate::DataConnReq - [&](const std::string& connect_host, int connect_port) {
      std::string net_gate_url{
        current_net_gate_routing_id.data<char>(),
        current_net_gate_routing_id.size() - 2
      };
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
        [&](const RemoteActorHandle&) {
          throw ZAFException("Failed to register actor with name ", name,
            " because it is not a local actor.");
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
      requesters.push_back(this->take_current_message());
    },
    NetGate::RemoteActorLookupReq - [&](const std::string& name) {
      std::string peer{
        current_net_gate_routing_id.data<char>(),
        current_net_gate_routing_id.size() - 2
      };
      auto& local_actor = local_registered_actors[name];
      if (local_actor.actor) {
        this->send_to_net_gate(peer,
          NetGate::RemoteActorLookupRep, name, *local_actor.actor);
      } else {
        local_actor.requesters.push_back(std::move(peer));
      }
    },
    NetGate::RemoteActorLookupRep - [&](const std::string& name, const LocalActorHandle& remote_actor) {
      std::string peer{
        current_net_gate_routing_id.data<char>(),
        current_net_gate_routing_id.size() - 2
      };
      auto& conn = net_gate_connections.at(peer);
      auto& requesters = conn.actor_lookup_requesters;
      auto iter = requesters.find(name);
      Actor actor{RemoteActorHandle{conn.net_sender_info, remote_actor}};
      for (auto& i : iter->second) {
        this->reply(*i, ActorLookupRep, peer, name, actor);
      }
      requesters.erase(iter);
    },
    NetGate::Ping - [&]() {
      std::string peer{
        current_net_gate_routing_id.data<char>(),
        current_net_gate_routing_id.size() - 2
      };
      this->establish_connection(peer);
      this->imme_send_to_net_gate(peer, NetGate::Pong);
    },
    NetGate::Pong - [&]() {
      std::string peer{
        current_net_gate_routing_id.data<char>(),
        current_net_gate_routing_id.size() - 2
      };
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
      if (info.net_gate_url == bind_url) {
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
  if (bool succ = false; !try_receive_guard([&]() {
    succ = bool(net_recv_socket.recv(current_net_gate_routing_id, zmq::recv_flags::none));
  }) || !succ) {
    return;
  }
  zmq::message_t msg_bytes;
  receive_guard([&]() {
    if (!net_recv_socket.recv(msg_bytes, zmq::recv_flags::none)) {
      throw ZAFException("Receive empty message from ", current_net_gate_routing_id);
    }
  });
  Deserializer s(msg_bytes.data<char>());
  auto msg_code = deserialize<size_t>(s);
  // not reading msg_type here because it only receives Message::Type::Normal messages
  auto types_hash = deserialize<size_t>(s);
  TypedMessage<TypedSerializedMessageBody<zmq::message_t>> msg{
    Actor{},
    msg_code,
    types_hash,
    std::move(msg_bytes),
    sizeof(size_t) + sizeof(size_t)
  };
  handlers.process(msg);
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
    static_cast<LocalActorHandle&>(conn.sender), bind_url, url
  };
  conn.receiver = actor_sys.spawn<Receiver>(bind_host, conn.net_sender_info);
  actor_sys.inc_num_detached_actors();
  // 3. Ask the receiver which port it binds
  auto& r = iter_ins.first->second.receiver;
  this->request(r, NetGate::BindPortReq).on_reply({
    NetGate::BindPortRep - [&](int port) {
      this->send_to_net_gate(url, NetGate::DataConnReq, this->bind_host, port);
    }
  });
  return true;
}

void NetGate::NetGateActor::launch() {
  thread::set_name(to_string("ZAF/NG", this->get_actor_id()));
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
  if (bool(net_send_socket)) {
    return;
  }
  this->ActorBehaviorX::initialize_send_socket();
  net_send_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::router);
  this->initialize_recv_socket();
  auto routing_id = to_string(bind_host, ':', bind_port, "/s");
  net_send_socket.set(zmq::sockopt::routing_id, zmq::buffer(routing_id));
  net_send_socket.set(zmq::sockopt::sndhwm, 0);
  // net_send_socket.set(zmq::sockopt::linger, 0);
}

void NetGate::NetGateActor::initialize_recv_socket() {
  if (bool(net_recv_socket)) {
    return;
  }
  this->ActorBehaviorX::initialize_recv_socket();
  net_recv_socket = zmq::socket_t(this->get_actor_system().get_zmq_context(), zmq::socket_type::router);
  if (bind_port == 0) {
    while (true) {
      // randomly generate a port within [10000, 65536) and try
      auto rand_bind_port = rand() % (65536 - 10000) + 10000;
      auto routing_id = to_string(bind_host, ':', rand_bind_port, "/r");
      net_recv_socket.set(zmq::sockopt::routing_id, zmq::buffer(routing_id));
      auto bind_url = to_string("tcp://", bind_host, ':', rand_bind_port);
      try {
        net_recv_socket.bind(bind_url);
      } catch (...) {
        // re-try
        continue;
      }
      bind_port = rand_bind_port;
      break;
    }
  } else {
    auto routing_id = to_string(bind_host, ':', bind_port, "/r");
    net_recv_socket.set(zmq::sockopt::routing_id, zmq::buffer(routing_id));
    auto bind_url = to_string("tcp://", bind_host, ':', bind_port);
    net_recv_socket.bind(bind_url);
  }
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

void NetGate::initialize(ActorSystem& actor_sys, const std::string& bind_host, int bind_port) {
  if (this->actor_sys) {
    throw ZAFException("Attempt to initialize an already initialized NetGate");
  }
  this->actor_sys = &actor_sys;
  // host and port are stored inside NetGateActor
  // because the NetGate object may be destroyed before NetGateActor
  net_gate_actor = actor_sys.spawn<NetGateActor>(bind_host, bind_port);
}

void NetGate::terminate() {
  if (!this->actor_sys) {
    return;
  }
  auto sender = this->actor_sys->create_scoped_actor();
  sender->send(net_gate_actor, NetGate::Termination);
  this->net_gate_actor = nullptr;
}

const Actor& NetGate::actor() const {
  if (!this->actor_sys) {
    throw ZAFException("NetGate is used before being initialized.");
  }
  return net_gate_actor;
}
} // namespace zaf
