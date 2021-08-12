#pragma once

#include <cstddef>
#include <functional>
#include <variant>

#include "macros.hpp"

namespace zaf {
struct LocalActorHandle {
  LocalActorHandle() = default;
  LocalActorHandle(const LocalActorHandle&) = default;
  LocalActorHandle& operator=(const LocalActorHandle&) = default;

  LocalActorHandle(ActorIdType);
  operator bool() const;
  friend bool operator==(const LocalActorHandle&, const LocalActorHandle&);
  size_t hash_code() const;

  ActorIdType local_actor_id{0};
};

struct RemoteActorHandle {
  RemoteActorHandle() = default;
  RemoteActorHandle(const RemoteActorHandle&) = default;
  RemoteActorHandle& operator=(const RemoteActorHandle&) = default;

  RemoteActorHandle(ActorIdType, ActorIdType);
  operator bool() const;
  friend bool operator==(const RemoteActorHandle& a, const RemoteActorHandle& b);
  size_t hash_code() const;

  ActorIdType net_sender_id{0};
  ActorIdType remote_actor_id{0};
};

class Actor {
public:
  Actor() = default;
  Actor(const Actor&) = default;
  Actor(Actor&&) = default;
  Actor& operator=(const Actor& other) = default;
  Actor& operator=(Actor&& other) = default;

  Actor(const LocalActorHandle&);
  Actor(const RemoteActorHandle&);
  Actor(std::nullptr_t);
  Actor& operator=(std::nullptr_t);

  // Return the id of the actor who should receives the
  // message when sending a message via this Actor
  ActorIdType get_actor_id() const;

  operator bool() const;
  friend bool operator==(const Actor&, const Actor&);
  size_t hash_code() const;

  bool is_remote() const;
  bool is_local() const;

  template<typename Visitor>
  inline void visit(Visitor&& v) const { std::visit(v, handle); }

  template<typename Visitor>
  inline void visit(Visitor&& v) { std::visit(v, handle); }

  operator LocalActorHandle&();
  operator const LocalActorHandle&() const;
  operator RemoteActorHandle&();
  operator const RemoteActorHandle&() const;

private:
  std::variant<
    LocalActorHandle,
    RemoteActorHandle
  > handle;
};
} // namespace zaf

namespace std {
template<>
struct hash<zaf::Actor> {
  inline size_t operator()(const zaf::Actor& actor) const {
    return actor.hash_code();
  }
};
} // namespace std
