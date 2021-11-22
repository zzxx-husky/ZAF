#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "actor.hpp"
#include "actor_behavior_x.hpp"
#include "actor_system.hpp"
#include "scoped_actor.hpp"

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
  inline constexpr static size_t NetGateCodeBase = (size_t(~0u) - 1) << (sizeof(size_t) << 2);
  inline constexpr static Code NetGateConnReq        {NetGateCodeBase + 1};
  inline constexpr static Code DataConnReq           {NetGateCodeBase + 2};
  inline constexpr static Code Termination           {NetGateCodeBase + 3};
  inline constexpr static Code FlushBuffer           {NetGateCodeBase + 4};
  inline constexpr static Code ActorRegistration     {NetGateCodeBase + 5};
  inline constexpr static Code ActorLookupReq        {NetGateCodeBase + 6};
  inline constexpr static Code ActorLookupRep        {NetGateCodeBase + 7};
  inline constexpr static Code RemoteActorLookupRep  {NetGateCodeBase + 8};
  inline constexpr static Code RemoteActorLookupReq  {NetGateCodeBase + 9};
  inline constexpr static Code BindPortReq           {NetGateCodeBase + 10};
  inline constexpr static Code BindPortRep           {NetGateCodeBase + 11};
  inline constexpr static Code Ping                  {NetGateCodeBase + 12};
  inline constexpr static Code Pong                  {NetGateCodeBase + 13};
  inline constexpr static Code PingRetry             {NetGateCodeBase + 14};
  inline constexpr static Code RetrieveActorReq      {NetGateCodeBase + 15};
  inline constexpr static Code RetrieveActorRep      {NetGateCodeBase + 16};
  inline constexpr static Code NetGateBindPortReq    {NetGateCodeBase + 17};
  inline constexpr static Code NetGateBindPortRep    {NetGateCodeBase + 18};

private:
  class Receiver : public ActorBehaviorX {
  public:
    Receiver(const std::string bind_host, const NetSenderInfo& net_sender_info);

    MessageHandlers behavior() override;

    void receive_once_from_net();

    void launch() override;

    void initialize_recv_socket() override;
    void terminate_recv_socket() override;

  protected:
    const std::string bind_host;
    zmq::socket_t net_recv_socket;
    const NetSenderInfo& net_sender_info;
  };

  class Sender : public ActorBehaviorX {
  public:
    MessageHandlers behavior() override;

    void initialize_send_socket() override;
    void terminate_send_socket() override;

    void push_to_buffer(MessageBytes& content);
    void flush_byte_buffer();
    void post_swsr_consumption() override;

    std::string get_name() const override;

  protected:
    std::vector<char> byte_buffer;
    unsigned num_buffered_messages = 0;

    std::string connected_url;
    zmq::socket_t net_send_socket;
    bool forward_any_message = true;

    std::vector<MessageBytes> pending_messages;
  };

  /**
   * Supposed NetGateActor binds to tcp://a.b.c.d:port
   * The routing id of net_send_socket: a.b.c.d:port/s
   * The routing id of net_recv_socket: a.b.c.d:port/r
   * The key stored in peer's map: a.b.c.d:port
   * The url that Receivers created by this NetGateActor bind is a.b.c.d:*
   **/
  class NetGateActor : public ActorBehaviorX {
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
        conn.pending_messages.emplace_back(std::move(zmq::message_t{&bytes.front(), bytes.size()}));
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
      NetSenderInfo net_sender_info;
      // whether `pong` message is received, i.e., whether the connection is well established.
      bool is_ponged = false;
      // remote actor name -> request message (which contains the requesters)
      std::unordered_map<
        std::string,
        std::vector<Message>
      > actor_lookup_requesters;
      // a buffer storing messages that are pending and waiting for the `pong` message from peer net gate
      std::vector<zmq::message_t> pending_messages;

      NetGateConn() = default;
      NetGateConn(NetGateConn&&) = default;
      NetGateConn(const NetGateConn&) = delete;
    };
    // net_gate urls -> Sender/Receiver
    std::unordered_map<std::string, NetGateConn> net_gate_connections;
    struct LocalActor {
      // local registered actor, requests may come before local actor is registered
      std::optional<LocalActorHandle> actor;
      // remote actor requesters
      std::vector<std::string> requesters;
    };
    // name -> local actor
    std::unordered_map<std::string, LocalActor> local_registered_actors;
    zmq::message_t current_net_gate_routing_id;
    // for communication with peer NetGateActors
    zmq::socket_t net_recv_socket;
    zmq::socket_t net_send_socket;
    // this NetGateActor will listen to "tcp://bind_host:bind_port"
    // Receivers will listen to "tcp://bind_host:*"
    std::string bind_host;
    int bind_port = 0;
    std::string bind_url; // which is bind_host:bind_port
  };

public:
  NetGate() = default;
  NetGate(ActorSystem& actor_sys, const std::string& bind_host, int port);

  void initialize(ActorSystem& actor_sys, const std::string& bind_host, int port);
  void terminate();

  const Actor& actor() const;

private:
  ActorSystem* actor_sys = nullptr;
  Actor net_gate_actor = nullptr;
};
} // namespace zaf
