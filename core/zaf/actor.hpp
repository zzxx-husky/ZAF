#pragma once

#include <cstddef>
#include <iostream>
#include <functional>
#include <string>
#include <variant>

#include "macros.hpp"

namespace zaf {
struct LocalActorHandle {
  LocalActorHandle() = default;
  LocalActorHandle(const LocalActorHandle&) = default;
  LocalActorHandle& operator=(const LocalActorHandle&) = default;

  LocalActorHandle(ActorIdType id, bool use_swsr);
  // to avoid LocalActorHandle is converted to bool and then to other numeric types implicitly
  explicit operator bool() const;
  friend bool operator==(const LocalActorHandle&, const LocalActorHandle&);
  size_t hash_code() const;
  friend std::ostream& operator<<(std::ostream&, const LocalActorHandle&);

  ActorIdType local_actor_id{0};
  bool use_swsr_msg_delivery{false};

  inline constexpr static size_t SerializationSize = sizeof(ActorIdType) + sizeof(bool);
};

struct NetSenderInfo {
  // the net sender
  LocalActorHandle net_sender;
  // the url of the local net gate that creates the net sender
  std::string local_net_gate_url;
  // the url of the remote net gate that creates the net receiver who receives the messages of this net sender
  std::string remote_net_gate_url;

  friend std::ostream& operator<<(std::ostream&, const NetSenderInfo&);
};

struct RemoteActorHandle {
  RemoteActorHandle() = default;
  RemoteActorHandle(const RemoteActorHandle&) = default;
  RemoteActorHandle& operator=(const RemoteActorHandle&) = default;

  RemoteActorHandle(const NetSenderInfo&, const LocalActorHandle&);
  // to avoid RemoteActorHandle is converted to bool and then to other numeric types implicitly
  explicit operator bool() const;
  friend bool operator==(const RemoteActorHandle& a, const RemoteActorHandle& b);
  size_t hash_code() const;
  friend std::ostream& operator<<(std::ostream&, const RemoteActorHandle&);

  // `net_sender_info` is a reference
  const NetSenderInfo* net_sender_info = nullptr;
  LocalActorHandle remote_actor;
};

/**
 * Store necessary information used to obtain a remote actor
 **/
struct ActorInfo {
  // the actor, may be a local one or a remote one
  LocalActorHandle actor;
  // the url of the net gate to connect if sending a message to the actor;
  // if the actor is a RemoteActorHandle, the url should be the url of handle.net_sender_info->remote_net_gate_url;
  // if the actor is a LocalActorHandle, the url should be the url of the local net gate who receive the message that requests the actor
  std::string net_gate_url;
};

class Actor {
public:
  Actor() = default;
  Actor(const Actor&) = default;
  Actor(Actor&&) = default;
  Actor& operator=(const Actor& other) = default;
  Actor& operator=(Actor&& other) = default;

  explicit Actor(const LocalActorHandle&);
  explicit Actor(const RemoteActorHandle&);
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
  inline decltype(auto) visit(Visitor&& v) const {
    return std::visit(v, handle);
  }

  template<typename Visitor>
  inline decltype(auto) visit(Visitor&& v) {
    return std::visit(v, handle);
  }

  explicit operator LocalActorHandle&();
  explicit operator const LocalActorHandle&() const;
  explicit operator RemoteActorHandle&();
  explicit operator const RemoteActorHandle&() const;

  ActorInfo to_actor_info(const std::string& local_net_gate_url) const;
  ActorInfo to_actor_info(const Actor& requester) const;

  friend std::ostream& operator<<(std::ostream&, const Actor&);

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
