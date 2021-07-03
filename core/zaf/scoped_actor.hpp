#pragma once

#include <memory>

#include "actor_behavior.hpp"

namespace zaf {
class ScopedActor {
public:
  ScopedActor(ActorBehavior* actor);

  ScopedActor(const ScopedActor&) = delete;
  ScopedActor(ScopedActor&&) = default;

  ActorBehavior* operator->();

private:
  std::unique_ptr<ActorBehavior> actor;
};
} // namespace zaf
