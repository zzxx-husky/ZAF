#pragma once

#include "net_gate.hpp"

namespace zaf {

/**
 * Usage:
 * 1. Lookup actor
 *    send: (NetGate::ActorLookupReq, std::string host:port, std::string name)
 *          will automatically connect to host:port if that is not connected
 *    recv: (NetGate::ActorLookupRep, std::string host:port, std::string name, Actor actor)
 * 2. Register actor
 *    NetGate::register_actor(std::string name, actor);
 * 3. Retrieve Actor based on ActorInfo
 * 4. Bind port lookup
 **/
struct NetGateClient {
  const Actor net_gate_actor;

  // 1. lookup actor, request
  template<typename ActorLike>
  inline decltype(auto) lookup_actor(ActorLike&& self, const std::string& url, const std::string& name) const {
    return self.send(net_gate_actor, NetGate::ActorLookupReq, url, name);
  }

  template<typename Handler,
    typename Sig = traits::is_callable<Handler>,
    std::enable_if_t<Sig::value>* = nullptr,
    std::enable_if_t<std::is_invocable_v<Handler, std::string&, std::string&, Actor&>>* = nullptr>
  inline auto on_lookup_actor_reply(Handler&& handler) const {
    return NetGate::ActorLookupRep - std::forward<Handler>(handler);
  }

  template<typename Handler,
    typename Sig = traits::is_callable<Handler>,
    std::enable_if_t<Sig::value>* = nullptr,
    std::enable_if_t<std::is_invocable_v<Handler, Actor&>>* = nullptr>
  inline auto on_lookup_actor_reply(Handler&& handler) const {
    return NetGate::ActorLookupRep -
    [h = std::forward<Handler>(handler)](std::string&, std::string&, Actor& actor) {
      h(actor);
    };
  }

  // 2. register actor
  template<typename ActorLike>
  inline decltype(auto) register_actor(ActorLike&& self, const std::string& name, const Actor& actor) const {
    return self.send(net_gate_actor, NetGate::ActorRegistration, name, actor);
  }

  // 3. ActorInfo -> Actor
  template<typename ActorLike>
  inline decltype(auto) retrieve_actor(ActorLike&& self, const ActorInfo& info) const {
    return self.send(net_gate_actor, NetGate::RetrieveActorReq, info);
  }

  template<typename Handler,
    typename Sig = traits::is_callable<Handler>,
    std::enable_if_t<Sig::value>* = nullptr,
    std::enable_if_t<std::is_invocable_v<Handler, ActorInfo&, Actor&>>* = nullptr>
  inline auto on_retrieve_actor_reply(Handler&& handler) const {
    return NetGate::RetrieveActorRep - std::forward<Handler>(handler);
  }

  template<typename Handler,
    typename Sig = traits::is_callable<Handler>,
    std::enable_if_t<Sig::value>* = nullptr,
    std::enable_if_t<std::is_invocable_v<Handler, Actor&>>* = nullptr>
  inline auto on_retrieve_actor_reply(Handler&& handler) const {
    return NetGate::RetrieveActorRep -
      [h = std::forward<Handler>(handler)](ActorInfo&, Actor& a) {
        h(a);
      };
  }

  // 4. Query the port NetGate binds on
  template<typename ActorLike>
  inline decltype(auto) request_bind_port(ActorLike&& self) const {
    return self.send(net_gate_actor, NetGate::NetGateBindPortReq);
  }

  template<typename Handler,
    typename Sig = traits::is_callable<Handler>,
    std::enable_if_t<Sig::value>* = nullptr,
    std::enable_if_t<std::is_invocable_v<Handler, int>>* = nullptr>
  inline auto on_bind_port_reply(Handler&& handler) const {
    return NetGate::NetGateBindPortRep - std::forward<Handler>(handler);
  }

  // 5. Connect this net gate to another net gate
  template<typename ActorLike>
  inline void connect_to_net_gate(ActorLike&& self, const std::string& host, int port) {
    self.send(net_gate_actor, NetGate::NetGateConnReq, host, port);
  }

  // 6. Register actor
  template<typename ActorLike>
  inline void register_actor(ActorLike&& self, const std::string& name, const Actor& actor) {
    self.send(net_gate_actor, NetGate::ActorRegistration, name, actor);
  }

  // 7. Terminate this net gate
  template<typename ActorLike>
  inline void terminate(ActorLike&& self) {
    self.send(net_gate_actor, NetGate::Termination);
  }
};

} // namespace zaf
