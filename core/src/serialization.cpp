#include "zaf/actor.hpp"
#include "zaf/actor_system.hpp"
#include "zaf/net_gate.hpp"
#include "zaf/net_gate_client.hpp"
#include "zaf/serializer.hpp"

namespace zaf {
void serialize(Serializer& s, const LocalActorHandle& l) {
  s.write(l.local_actor_id).write(l.use_swsr_msg_delivery);
}

void deserialize(Deserializer& s, LocalActorHandle& l) {
  s.read(l.local_actor_id).read(l.use_swsr_msg_delivery);
}

void serialize(Serializer& s, const ActorInfo& a) {
  s.write(a.net_gate_url).write(a.actor);
}

void deserialize(Deserializer& s, ActorInfo& a) {
  s.read(a.net_gate_url).read(a.actor);
}

void serialize(Serializer& s, const std::vector<bool>& v) {
  s.write(v.size());
  for (auto i : v) {
    s.write(bool(i));
  }
}

void deserialize(Deserializer& s, std::vector<bool>& v) {
  auto size = s.read<size_t>();
  v.resize(size);
  for (size_t i = 0; i < size; i++) {
    v[i] = s.read<bool>();
  }
}
} // namespace zaf
