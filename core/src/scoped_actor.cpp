#include "zaf/scoped_actor.hpp"

namespace zaf {
ScopedActor::ScopedActor(ActorBehavior* actor):
  actor(actor) {
}

ScopedActor& ScopedActor::operator=(std::nullptr_t) {
  actor = nullptr;
  return *this;
}

ScopedActor& ScopedActor::operator=(ScopedActor&& other) {
  actor = std::move(other.actor);
  return *this;
}

ActorBehavior* ScopedActor::operator->() {
  return actor.get();
}
} // namespace zaf

