#pragma once

#include <cstddef>
#include <functional>

#include "macros.hpp"

namespace zaf {

class Actor {
public:
  Actor() = default;
  Actor(const Actor&) = default;
  Actor(Actor&&) = default;

  Actor(ActorIdType actor_id);

  Actor& operator=(const Actor& other);

  Actor& operator=(Actor&& other);

  ActorIdType get_actor_id() const;

  operator bool() const;

  friend bool operator==(const Actor&, const Actor&);

private:
  // 0 means a null actor
  ActorIdType actor_id = 0;
};
} // namespace zaf

namespace std {
template<>
struct hash<zaf::Actor> {
  inline size_t operator()(const zaf::Actor& actor) const {
    return {actor.get_actor_id()};
  }
};
} // namespace std
