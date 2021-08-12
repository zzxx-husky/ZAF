#include "zaf/actor.hpp"
#include "zaf/hash.hpp"

namespace zaf {
LocalActorHandle::LocalActorHandle(ActorIdType id):
  local_actor_id(id) {
}

LocalActorHandle::operator bool() const {
  return local_actor_id;
}

bool operator==(const LocalActorHandle& a, const LocalActorHandle& b) {
  return a.local_actor_id == b.local_actor_id;
}

size_t LocalActorHandle::hash_code() const {
  return local_actor_id;
}

RemoteActorHandle::RemoteActorHandle(ActorIdType net_sender_id, ActorIdType remote_actor_id):
  net_sender_id(net_sender_id),
  remote_actor_id(remote_actor_id) {
}

RemoteActorHandle::operator bool() const {
  return net_sender_id && remote_actor_id;
}

bool operator==(const RemoteActorHandle& a, const RemoteActorHandle& b) {
  return a.net_sender_id == b.net_sender_id && a.remote_actor_id == b.remote_actor_id;
}

size_t RemoteActorHandle::hash_code() const {
  return hash_combine(net_sender_id, remote_actor_id);
}

Actor::Actor(const LocalActorHandle& x): handle(x) {}

Actor::Actor(const RemoteActorHandle& x): handle(x) {}

Actor::Actor(std::nullptr_t): Actor() {}

Actor& Actor::operator=(std::nullptr_t) {
  handle = {};
  return *this;
}

ActorIdType Actor::get_actor_id() const {
  if (handle.index() == 0) {
    return std::get<0>(handle).local_actor_id;
  } else {
    return std::get<1>(handle).net_sender_id;
  }
}

Actor::operator bool() const {
  return std::visit([](auto&& x) { return bool(x); }, handle);
}

bool operator==(const Actor& a, const Actor& b) {
  return a.handle == b.handle;
}

size_t Actor::hash_code() const {
  return std::visit([](auto&& x) { return x.hash_code(); }, handle);
}

bool Actor::is_local() const {
  return handle.index() == 0;
}

bool Actor::is_remote() const {
  return !is_local();
}

Actor::operator LocalActorHandle&() { return std::get<LocalActorHandle>(handle); }

Actor::operator const LocalActorHandle&() const { return std::get<LocalActorHandle>(handle); }

Actor::operator RemoteActorHandle&() { return std::get<RemoteActorHandle>(handle); }

Actor::operator const RemoteActorHandle&() const { return std::get<RemoteActorHandle>(handle); }

} // namespace zaf
