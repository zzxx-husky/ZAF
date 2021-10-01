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
 **/
struct NetGateClient {
  const Actor net_gate_actor;

  // 1. lookup actor, request
  template<typename ActorLike>
  inline decltype(auto) lookup_actor(ActorLike&& self, const std::string& url, const std::string& name) const {
    return self.send(net_gate_actor, NetGate::ActorLookupReq, url, name);
  }

  // 1. lookup actor, reply 
  template<typename Handler,
    typename Sig = traits::is_callable<Handler>,
    typename std::enable_if_t<Sig::value>* = nullptr,
    typename std::enable_if_t<std::is_invocable_v<Handler, std::string&, std::string&, Actor&>>* = nullptr>
  inline auto on_lookup_actor_reply(Handler&& handler) const {
    return NetGate::ActorLookupRep - std::forward<Handler>(handler);
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
    typename std::enable_if_t<Sig::value>* = nullptr,
    typename std::enable_if_t<std::is_invocable_v<Handler, ActorInfo&, Actor&>>* = nullptr>
  inline auto on_retrieve_actor_reply(Handler&& handler) const {
    return NetGate::RetrieveActorRep - std::forward<Handler>(handler);
  }
};

} // namespace zaf
