#include "zaf/actor.hpp"
#include "zaf/hash.hpp"

namespace zaf {
LocalActorHandle::LocalActorHandle(ActorIdType id, bool use_swsr):
  local_actor_id(id),
  use_swsr_msg_delivery(use_swsr) {
}

LocalActorHandle::operator bool() const {
  return local_actor_id;
}

bool LocalActorHandle::operator==(const LocalActorHandle& b) const {
  return this->local_actor_id == b.local_actor_id;
}

size_t LocalActorHandle::hash_code() const {
  return local_actor_id;
}

RemoteActorHandle::RemoteActorHandle(const NetSenderInfo& net_sender_info,
  const LocalActorHandle& remote_actor):
  net_sender_info(&net_sender_info),
  remote_actor(remote_actor) {
}

RemoteActorHandle::operator bool() const {
  return net_sender_info;
}

bool RemoteActorHandle::operator==(const RemoteActorHandle& b) const {
  return net_sender_info == b.net_sender_info &&
    remote_actor == b.remote_actor;
}

size_t RemoteActorHandle::hash_code() const {
  return hash_combine(net_sender_info->net_sender.local_actor_id, remote_actor.hash_code());
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
    return std::get<1>(handle).net_sender_info->net_sender.local_actor_id;
  }
}

Actor::operator bool() const {
  return std::visit([](auto&& x) { return static_cast<bool>(x); }, handle);
}

bool Actor::operator==(const Actor& b) const {
  return handle == b.handle;
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

Actor::operator LocalActorHandle&() {
  return std::get<LocalActorHandle>(handle);
}

Actor::operator const LocalActorHandle&() const {
  return std::get<LocalActorHandle>(handle);
}

Actor::operator RemoteActorHandle&() {
  return std::get<RemoteActorHandle>(handle);
}

Actor::operator const RemoteActorHandle&() const {
  return std::get<RemoteActorHandle>(handle);
}

ActorInfo Actor::to_actor_info(const Actor& requester) const {
  return this->visit(overloaded{
    [&](const LocalActorHandle& l) {
      std::string url = requester.visit(overloaded{
        [&](const LocalActorHandle& l) {
          // a local actor requests the actor info of another local actor
          return std::string("");
        },
        [&](const RemoteActorHandle& r) {
          // a remote actor requests the actor info of a local actor
          return r.net_sender_info->local_net_gate_url;
        }
      });
      return ActorInfo{l, url};
    },
    [&](const RemoteActorHandle& r) {
      // a remote actor requests the actor info of a remote actor
      return ActorInfo{r.remote_actor, r.net_sender_info->remote_net_gate_url};
    }
  });
}

std::ostream& operator<<(std::ostream& out, const LocalActorHandle& a) {
  if (a.local_actor_id) {
    out << "Local{id: " << a.local_actor_id << ", swsr: " << a.use_swsr_msg_delivery << '}';
  } else {
    out << "Local{nullptr}";
  }
  return out;
}

std::ostream& operator<<(std::ostream& out, const NetSenderInfo& i) {
  out << "NetSender{sender: " << i.net_sender << ", local url: " << i.local_net_gate_url
    << ", remote url: " << i.remote_net_gate_url << '}';
  return out;
}

std::ostream& operator<<(std::ostream& out, const RemoteActorHandle& a) {
  out << "Remote{actor: " << a.remote_actor << ", net_sender: " << *a.net_sender_info << '}';
  return out;
}

std::ostream& operator<<(std::ostream& out, const Actor& a) {
  a.visit(overloaded{
    [&](const LocalActorHandle& l) {
      out << l;
    },
    [&](const RemoteActorHandle& r) {
      out << r;
    }
  });
  return out;
}

} // namespace zaf
