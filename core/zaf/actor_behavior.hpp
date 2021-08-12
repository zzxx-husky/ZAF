#pragma once

#include <cstdint>

#include "actor.hpp"
#include "message_handlers.hpp"
#include "zaf_exception.hpp"

#include "zmq.hpp"

namespace zaf {
class ActorSystem;
class ActorGroup;

class ActorBehavior {
public:
  ActorBehavior() = default;

  /**
   * To be overrided by subclass
   **/
  virtual void start();
  virtual void stop();
  virtual MessageHandlers behavior();

  /**
   * To be used by subclass
   **/
  template<typename ... ArgT>
  inline void reply(size_t code, ArgT&& ... args) {
    this->send(this->get_current_message().get_sender_actor(), code, std::forward<ArgT>(args)...);
  }

  template<typename ... ArgT>
  inline void send(ActorBehavior& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver.get_actor_id(), code, std::forward<ArgT>(args)...);
  }

  template<typename ... ArgT>
  inline void send(const Actor& receiver, size_t code, ArgT&& ... args) {
    if (!receiver) {
      return; // ignore message sent to null actor
    }
    receiver.visit(overloaded {
      [&](const LocalActorHandle& r) {
        auto message = make_message(LocalActorHandle{this->get_actor_id()}, code, std::forward<ArgT>(args)...);
        this->send(r, message);
      },
      [&](const RemoteActorHandle& r) {
        if constexpr (traits::all_serializable<ArgT ...>::value) {
          std::vector<char> bytes;
          Serializer(bytes)
            .write(this->get_actor_id())
            .write(r.remote_actor_id)
            .write(code)
            .write(hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...))
            .write(std::forward<ArgT>(args) ...);
          // Have to copy the bytes here because we do not know how many bytes
          // are needed for serialization and we need to reserve more than necessary.
          this->send(r.net_sender_id, DefaultCodes::ForwardMessage,
            zmq::message_t{&bytes.front(), bytes.size()});
        } else {
          throw ZAFException("Attempt to serialize non-serializable data");
        }
      }
    });
  }

  template<typename ... ArgT>
  void send(const LocalActorHandle& receiver, size_t code, ArgT&& ... args) {
    auto m = make_message(LocalActorHandle{this->get_actor_id()}, code, std::forward<ArgT>(args)...);
    this->send(receiver, m);
  }

  void send(const LocalActorHandle& receiver, Message* m);

  // to process one incoming message with message handlers
  bool receive_once(MessageHandlers&& handlers, bool non_blocking = false);
  bool receive_once(MessageHandlers& handlers, bool non_blocking = false);
  bool receive_once(MessageHandlers&& handlers, const std::chrono::milliseconds&);
  bool receive_once(MessageHandlers& handlers, const std::chrono::milliseconds&);
  void receive(MessageHandlers&& handlers);
  void receive(MessageHandlers& handlers);

  ActorSystem& get_actor_system();
  ActorGroup& get_actor_group();
  Actor get_self_actor();

  Message& get_current_message();
  Actor get_current_sender_actor();

  void activate();
  void deactivate();
  bool is_activated() const;

  /**
   * To be used by ZAF
   **/
  virtual void initialize_actor(ActorSystem& sys, ActorGroup& group);
  virtual void terminate_actor();

  void initialize_routing_id_buffer();

  virtual void initialize_recv_socket();
  virtual void terminate_recv_socket();

  virtual void initialize_send_socket();
  virtual void terminate_send_socket();

  zmq::socket_t& get_recv_socket();
  zmq::socket_t& get_send_socket();

  virtual void launch();

  virtual ~ActorBehavior();

  ActorIdType get_actor_id() const;

protected:
  void connect(ActorIdType peer);
  void disconnect(ActorIdType peer);

  std::string routing_id_buffer;
  const std::string& get_routing_id(ActorIdType id, bool send_or_recv);

  DefaultHashSet<ActorIdType> connected_receivers;
  bool activated = false;
  zmq::message_t current_sender_routing_id;
  Message* current_message = nullptr;

  ActorIdType actor_id;
  zmq::socket_t send_socket, recv_socket;
  ActorGroup* actor_group_ptr = nullptr;
  ActorSystem* actor_system_ptr = nullptr;
};
} // namespace zaf
