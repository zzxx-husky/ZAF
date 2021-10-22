#pragma once

#include <utility>
#include <vector>

#include "actor_behavior.hpp"
#include "code.hpp"
#include "swsr_delivery_queue.hpp"

namespace zaf {
// TODO(zzxx): support running ActorBehaviorX using ActorEngine
class ActorBehaviorX : public ActorBehavior {
public:
  ActorBehaviorX();

  void initialize_actor(ActorSystem&, ActorGroup&) override;

  LocalActorHandle get_local_actor_handle() const override;

  // send a normal message to message sender
  template<typename ... ArgT>
  inline void reply(size_t code, ArgT&& ... args) {
    if (this->get_current_message().get_type() == Message::Type::Request) {
      this->ActorBehavior::reply(code, std::forward<ArgT>(args) ...);
    } else {
      this->send(this->get_current_sender_actor(),
        code, Message::Type::Normal, std::forward<ArgT>(args)...);
    }
  }

  // send a message to a ActorBehavior
  template<typename ... ArgT>
  inline void send(const ActorBehavior& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver, code, Message::Type::Normal, std::forward<ArgT>(args) ...);
  }

  template<typename ... ArgT>
  inline void send(const ActorBehavior& receiver, size_t code, Message::Type type, ArgT&& ... args) {
    this->send(receiver.get_local_actor_handle(), code, type, std::forward<ArgT>(args)...);
  }

  // send a message to Actor, either remote or local
  template<typename ... ArgT>
  inline void send(const Actor& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver, code, Message::Type::Normal, std::forward<ArgT>(args) ...);
  }

  template<typename ... ArgT>
  void send(const Actor& receiver, size_t code, Message::Type type, ArgT&& ... args) {
    if (!receiver) {
      return;
    }
    receiver.visit(overloaded {
      [&](const LocalActorHandle& r) {
        auto message = make_message(this->get_local_actor_handle(), code, std::forward<ArgT>(args)...);
        message->set_type(type);
        this->send(r, message);
      },
      [&](const RemoteActorHandle& r) {
        if constexpr (traits::all_serializable<ArgT ...>::value) {
          std::vector<char> bytes;
          Serializer(bytes)
            .write(this->get_local_actor_handle())
            .write(r.remote_actor)
            .write(code)
            .write(type)
            .write(hash_combine(typeid(std::decay_t<ArgT>).hash_code() ...))
            .write(std::forward<ArgT>(args) ...);
          // Have to copy the bytes here because we do not know how many bytes
          // are needed for serialization and we need to reserve more than necessary.
          this->send(r.net_sender_info->net_sender,
            DefaultCodes::ForwardMessage, zmq::message_t{&bytes.front(), bytes.size()});
        } else {
          throw ZAFException("Attempt to serialize non-serializable message data.");
        }
      }
    });
  }

  // send a message to LocalActorHandle
  template<typename ... ArgT>
  inline void send(const LocalActorHandle& receiver, size_t code, ArgT&& ... args) {
    this->send(receiver, code, Message::Type::Normal, std::forward<ArgT>(args) ...);
  }

  template<typename ... ArgT>
  inline void send(const LocalActorHandle& receiver, size_t code, Message::Type type, ArgT&& ... args) {
    auto m = make_message(this->get_local_actor_handle(), code, std::forward<ArgT>(args)...);
    m->set_type(type);
    this->send(receiver, m);
  }

  void send(const LocalActorHandle& actor, Message* m);

  void register_swsr_queue(SWSRDeliveryQueue<Message*>* recv_queue);

  void notify_swsr_queue();

  void consume_swsr_recv_queues(MessageHandlers& handlers);

  void setup_swsr_connection(const Actor&);

  void setup_swsr_connection(const LocalActorHandle&);

  ~ActorBehaviorX();

private:
  // SWSRDeliveryQueue is created by sender, then delivered to and destroyed by the receiver
  // Receiver actor id -> message queue
  DefaultHashMap<ActorIdType, SWSRDeliveryQueue<Message*>*> swsr_send_queues;
  // Sender actor id -> message queue
  DefaultHashMap<ActorIdType, SWSRDeliveryQueue<Message*>*> swsr_recv_queues;
  // pointers in `active_recv_queues` points to the queues in `swsr_recv_queues`
  std::vector<SWSRDeliveryQueue<Message*>*> active_recv_queues;
  SWSRDeliveryQueue<Message*> self_swsr_queue;
};
} // namespace zaf
