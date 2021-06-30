#include "zaf/actor.hpp"

namespace zaf {
Actor::Actor(ActorIdType actor_id):
  actor_id(actor_id) {
}

Actor& Actor::operator=(const Actor& other) {
  actor_id = other.actor_id;
  return *this;
}

Actor& Actor::operator=(Actor&& other) {
  actor_id = std::move(other.actor_id);
  return *this;
}

ActorIdType Actor::get_actor_id() const {
  return this->actor_id;
}

bool operator==(const Actor& a, const Actor& b) {
  return a.actor_id == b.actor_id;
}
} // namespace zaf
