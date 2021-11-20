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
  inline void reply(Code code, ArgT&& ... args) {
    if (this->get_current_message().get_type() == Message::Type::Request) {
      this->ActorBehavior::reply(code, std::forward<ArgT>(args) ...);
    } else {
      this->send(this->get_current_sender_actor(),
        Message::Type::Normal, code, std::forward<ArgT>(args)...);
    }
  }

  // reply a message to the sender of the `msg`
  template<typename ... ArgT>
  inline void reply(Message& msg, Code code, ArgT&& ... args) {
    if (msg.get_type() == Message::Type::Request) {
      this->ActorBehavior::reply(msg, code, std::forward<ArgT>(args) ...);
    } else {
      this->send(msg.get_sender(), Message::Type::Normal, 
        code, std::forward<ArgT>(args) ...);
    }
  }

  // send a message to a ActorBehavior
  template<typename ... ArgT>
  inline void send(const ActorBehavior& receiver, Code code, ArgT&& ... args) {
    this->send(receiver, Message::Type::Normal, code, std::forward<ArgT>(args) ...);
  }

  template<typename ... ArgT>
  inline void send(const ActorBehavior& receiver, Message::Type type, Code code, ArgT&& ... args) {
    this->send(receiver.get_local_actor_handle(), type, code, std::forward<ArgT>(args)...);
  }

  // send a message to Actor, either remote or local
  template<typename ... ArgT>
  inline void send(const Actor& receiver, Code code, ArgT&& ... args) {
    this->send(receiver, Message::Type::Normal, code, std::forward<ArgT>(args) ...);
  }

  template<typename ... ArgT>
  void send(const Actor& receiver, Message::Type type, Code code, ArgT&& ... args) {
    if (!receiver) {
      return;
    }
    receiver.visit(overloaded {
      [&](const LocalActorHandle& r) {
        auto message = new_message(Actor{this->get_local_actor_handle()}, type,
          code, std::forward<ArgT>(args)...);
        this->send(r, message);
      },
      [&](const RemoteActorHandle& r) {
        if constexpr (traits::all_serializable<ArgT ...>::value) {
          auto bytes = MessageBytes::make(this->get_local_actor_handle(),
            r.remote_actor, type, code, std::forward<ArgT>(args) ...);
          this->send(r.net_sender_info->net_sender, Message::Type::Normal,
            DefaultCodes::ForwardMessage, std::move(bytes));
        } else {
          throw ZAFException("Attempt to serialize non-serializable message data: ",
            traits::NonSerializableAnalyzer<ArgT ...>::to_string());
        }
      }
    });
  }

  // send a message to LocalActorHandle
  template<typename ... ArgT>
  inline void send(const LocalActorHandle& receiver, Code code, ArgT&& ... args) {
    this->send(receiver, Message::Type::Normal, code, std::forward<ArgT>(args) ...);
  }

  template<typename ... ArgT>
  inline void send(const LocalActorHandle& receiver, Message::Type type, Code code, ArgT&& ... args) {
    auto m = new_message(Actor{this->get_local_actor_handle()}, type,
      code, std::forward<ArgT>(args)...);
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
