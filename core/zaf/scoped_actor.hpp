#pragma once

#include <memory>

#include "actor_behavior.hpp"

namespace zaf {
class ScopedActor {
public:
  ScopedActor() = default;
  ScopedActor(ActorBehavior* actor);

  ScopedActor(const ScopedActor&) = delete;
  ScopedActor(ScopedActor&&) = default;

  ScopedActor& operator=(std::nullptr_t);
  ScopedActor& operator=(ScopedActor&& other);

  ActorBehavior* operator->();

private:
  std::unique_ptr<ActorBehavior> actor = nullptr;
};
} // namespace zaf
