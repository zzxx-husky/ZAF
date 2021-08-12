#pragma once

#include <memory>

#include "actor_behavior.hpp"

namespace zaf {
template<typename ActorType>
class ScopedActor {
public:
  ScopedActor() = default;

  ScopedActor(ActorType* actor):
    actor(actor) {
  }

  ScopedActor(const ScopedActor<ActorType>&) = delete;
  ScopedActor(ScopedActor<ActorType>&&) = default;

  ScopedActor<ActorType>& operator=(std::nullptr_t) {
    actor = nullptr;
    return *this;
  }

  ScopedActor<ActorType>& operator=(ScopedActor<ActorType>&& other) {
    actor = std::move(other.actor);
    return *this;
  }

  ActorType* operator->() {
    return actor.get();
  }

  ActorType& operator*() {
    return *actor;
  }

  const ActorType& operator*() const {
    return *actor;
  }

private:
  std::unique_ptr<ActorType> actor = nullptr;
};
} // namespace zaf
