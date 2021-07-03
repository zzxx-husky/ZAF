#include "zaf/scoped_actor.hpp"

namespace zaf {
ScopedActor::ScopedActor(ActorBehavior* actor):
  actor(actor) {
}

ActorBehavior* ScopedActor::operator->() {
  return actor.get();
}
} // namespace zaf

