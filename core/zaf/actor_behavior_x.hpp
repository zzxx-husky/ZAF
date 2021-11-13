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
          auto bytes = MessageBytes::make(this->get_local_actor_handle(),
            r.remote_actor, code, type, std::forward<ArgT>(args) ...);
          this->send(r.net_sender_info->net_sender, DefaultCodes::ForwardMessage,
            Message::Type::Normal, std::move(bytes));
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

  std::string get_name() const override;

  void register_swsr_queue(std::shared_ptr<SWSRDeliveryQueue<Message*>>& recv_queue);

  void notify_swsr_queue();

  void consume_swsr_recv_queues(MessageHandlers& handlers);

  virtual void post_swsr_consumption();

  void setup_swsr_connection(const Actor&);

  void setup_swsr_connection(const LocalActorHandle&);

  ~ActorBehaviorX();

private:
  // Note(zzxx): Receiver may terminates before sender, we should use std::shared_ptr
  //   so that sender will destroy the queue if it is the case.
  // SWSRDeliveryQueue is created by sender, then delivered to and destroyed by the receiver
  // Receiver actor id -> message queue
  DefaultHashMap<ActorIdType, std::shared_ptr<SWSRDeliveryQueue<Message*>>> swsr_send_queues;
  // Sender actor id -> message queue
  DefaultHashMap<ActorIdType, std::shared_ptr<SWSRDeliveryQueue<Message*>>> swsr_recv_queues;
  // pointers in `active_recv_queues` points to the queues in `swsr_recv_queues`
  std::vector<std::pair<ActorIdType, SWSRDeliveryQueue<Message*>*>> active_recv_queues;
  // Make it as a shared_ptr so that it can be managed as like other queues
  std::shared_ptr<SWSRDeliveryQueue<Message*>> self_swsr_queue;
};
} // namespace zaf
