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
 **/
struct NetGateClient {
  const Actor net_gate_actor;

  inline void lookup_actor(ActorBehavior& self, const std::string& url, const std::string& name) const{
    self.send(net_gate_actor, NetGate::ActorLookupReq, url, name);
  }

  inline void register_actor(ActorBehavior& self, const std::string& name, const Actor& actor) const {
    self.send(net_gate_actor, NetGate::ActorRegistration, name, actor);
  }

  template<typename Handler,
    typename Sig = traits::is_callable<Handler>,
    typename std::enable_if_t<Sig::value>* = nullptr,
    typename std::enable_if_t<Sig::args_t::size == 3>* = nullptr,
    typename std::enable_if_t<std::is_same_v<typename Sig::args_t::template decay_arg_t<0>, std::string>>* = nullptr,
    typename std::enable_if_t<std::is_same_v<typename Sig::args_t::template decay_arg_t<1>, std::string>>* = nullptr,
    typename std::enable_if_t<std::is_same_v<typename Sig::args_t::template decay_arg_t<2>, Actor>>* = nullptr>
  inline auto on_lookup_actor_reply(Handler&& handler) const {
    return NetGate::ActorLookupRep - std::forward<Handler>(handler);
  }
};

} // namespace zaf
