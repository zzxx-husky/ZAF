#pragma once

#include <string>

#include "macros.hpp"

namespace zaf {
class ActorHandle {
public:
  static ActorHandle NullActorHandle{0};

  ActorHandle() = default;

  ActorHandle(const ActorHandle&) = default;

  ActorHandle(ActorIdType actor_id):
    ActorHandle(actor_id, true, actor_id) {
  }

  const ActorIdType actor_id{0};
  const bool is_local{true};
  const size_t hash_code;

protected:
  ActorHandle(ActorIdType actor_id, bool is_local, size_t hash_code):
    actor_id(actor_id),
    is_local(is_local),
    hash_code(hash_code) {
  }
};

struct RemoteActorHandle : public ActorHandle {
  RemoteActorHandle(ActorIdType actor_id,
      ActorIdType local_out_gate_actor_id,
      std::string remote_in_gate_routing_id):
    ActorHandle(actor_id, false, actor_id | ~std::hash<std::string>{}(remote_in_gate_routing_id)),
    local_out_gate_actor_id(local_out_gate_actor_id),
    remote_in_gate_routing_id(std::move(remote_in_gate_routing_id)) {
  }

  RemoteActorHandle(const RemoteActorHandle&) = default;

  const size_t local_out_gate_actor_id;
  const std::string remote_in_gate_routing_id;
};
} // namespace zaf
