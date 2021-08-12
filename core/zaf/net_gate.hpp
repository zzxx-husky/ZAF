#pragma once

#include "actor_system.hpp"
#include "scoped_actor.hpp"
#include "thread_based_actor_behavior.hpp"

namespace zaf {
/**
 * Considering using that a single thread with a single socket to
 * send messages to or receive messages from all the other machines
 * may not be a scalabile solution when we have a lot of machines,
 * the impl here uses a pair of push and pull sockets for the comm with one machine
 *
 * But this impl may also makes too many threads and sockets when there are too many machines.
 * Another impl is to use router socket in each thread in order to connect to multiple machines
 **/
class NetGate {
public:
  constexpr static Code NetGateConnReq{1};
  constexpr static Code DataConnReq{2};
  constexpr static Code Termination{3};
  constexpr static Code ActorRegistration{5};
  constexpr static Code ActorLookupReq{6};
  constexpr static Code ActorLookupRep{7};
  constexpr static Code RemoteActorLookupRep{8};
  constexpr static Code RemoteActorLookupReq{9};
  constexpr static Code BindPortReq{10};
  constexpr static Code BindPortRep{11};
  constexpr static Code Ping{12};
  constexpr static Code Pong{13};
  constexpr static Code PingRetry{14};

private:
  class Receiver : public ActorBehavior {
  public:
    Receiver(Actor sender, const std::string& bind_host);

    MessageHandlers behavior() override;

    void receive_once_from_net();

    void launch() override;

    void initialize_recv_socket() override;
    void terminate_recv_socket() override;

  private:
    const std::string bind_host;
    zmq::socket_t net_recv_socket;
    Actor sender;
  };

  class Sender : public ActorBehavior {
  public:
    MessageHandlers behavior() override;

    void initialize_send_socket() override;
    void terminate_send_socket() override;

    std::string connected_url;
  };

  /**
   * Supposed NetGateActor binds to tcp://a.b.c.d:port
   * The routing id of net_send_socket: a.b.c.d:port/s
   * The routing id of net_recv_socket: a.b.c.d:port/r
   * The key stored in peer's map: a.b.c.d:port
   * The url that Receivers created by this NetGateActor bind is a.b.c.d:*
   **/
  class NetGateActor : public ThreadBasedActorBehavior {
  public:
    NetGateActor(const std::string& host, int port);

    MessageHandlers behavior() override;

    // handle messages from peer NetGateActor
    void receive_once_from_net_gate(MessageHandlers& handlers);

    // send the data when the connection with the net gate peer at `ng_url` is stable
    template<typename ... ArgT>
    void send_to_net_gate(const std::string& ng_url, size_t msg_code, ArgT&& ... args) {
      std::vector<char> bytes;
      Serializer(bytes)
        .write(msg_code)
        .write(hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...))
        .write(std::forward<ArgT>(args) ...);
      auto& conn = net_gate_connections.at(ng_url);
      if (conn.is_ponged) {
        net_send_socket.send(zmq::buffer(ng_url + "/r"), zmq::send_flags::sndmore);
        net_send_socket.send(zmq::message_t{&bytes.front(), bytes.size()}, zmq::send_flags::none);
      } else {
        conn.pending_messages.push_back(std::move(zmq::message_t{&bytes.front(), bytes.size()}));
      }
    }

    // send the data immediately without ensuring the connection with the net gate peer at `ng_url` is stable
    // used to send Ping and Pong for checking stableness
    template<typename ... ArgT>
    void imme_send_to_net_gate(const std::string& ng_url, size_t msg_code, ArgT&& ... args) {
      std::vector<char> bytes;
      Serializer(bytes)
        .write(msg_code)
        .write(hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...))
        .write(std::forward<ArgT>(args) ...);
      auto& conn = net_gate_connections.at(ng_url);
      net_send_socket.send(zmq::buffer(ng_url + "/r"), zmq::send_flags::sndmore);
      net_send_socket.send(zmq::message_t{&bytes.front(), bytes.size()}, zmq::send_flags::none);
    }

    void ping_net_gate(const std::string&);
    void receive_pong(const std::string&);

    bool establish_connection(const std::string&);

    void launch() override;

    void initialize_send_socket() override;
    void terminate_send_socket() override;

    void initialize_recv_socket() override;
    void terminate_recv_socket() override;

    struct NetGateConn {
      Actor sender;
      Actor receiver;
      bool is_ponged = false;
      std::vector<zmq::message_t> pending_messages;
      std::unordered_map<std::string, std::vector<Actor>> actor_lookup_requesters;
    };
    // net_gate urls to Sender/Receiver
    std::unordered_map<std::string, NetGateConn> net_gate_connections;
    struct LocalActor {
      std::optional<Actor> actor;
      std::vector<std::string> requesters;
    };
    std::unordered_map<std::string, LocalActor> local_registered_actors;
    zmq::message_t current_net_gate_routing_id;
    // the host which NetGateActor and Receivers will bind
    const std::string bind_host;
    const int bind_port;
    // for communication with peer NetGateActors
    zmq::socket_t net_recv_socket;
    zmq::socket_t net_send_socket;
  };

public:
  NetGate() = default;
  NetGate(ActorSystem& actor_sys, const std::string& bind_host, int port);

  // may return null
  ActorSystem& get_actor_system();
  const ActorSystem& get_actor_system() const;

  // `url` in the format of "transport://address", e.g., "tcp://127.0.0.1:10086"
  void initialize(ActorSystem& actor_sys, const std::string& bind_host, int port);
  bool is_initialized() const;
  void terminate();

  void connect(const std::string& host, int port);

  void register_actor(const std::string& name, const Actor& actor);

  operator Actor() const;
  Actor actor() const;

private:
  Actor net_gate_actor = nullptr;
  ScopedActor<ActorBehavior> self = nullptr;
  ActorSystem* actor_sys = nullptr;
};
} // namespace zaf
