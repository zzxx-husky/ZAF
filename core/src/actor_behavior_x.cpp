#include "zaf/actor_behavior_x.hpp"

namespace zaf {
ActorBehaviorX::ActorBehaviorX() {
  ActorBehavior::inner_handlers.add_handlers(
    DefaultCodes::SWSRMsgQueueRegistration - [&](SWSRDeliveryQueue<Message*>* queue) {
      this->register_swsr_queue(queue);
      this->setup_swsr_connection(this->get_current_sender_actor());
    },
    DefaultCodes::SWSRMsgQueueNotification - [&]() {
      this->notify_swsr_queue();
    },
    DefaultCodes::SWSRMsgQueueConsumption - [&]() {
      this->consume_swsr_recv_queues(ActorBehavior::inner_handlers.get_child_handlers());
      if (!this->active_recv_queues.empty()) {
        this->ActorBehavior::send(*this, DefaultCodes::SWSRMsgQueueConsumption);
      }
    },
    DefaultCodes::SWSRMsgQueueTermination - [&]() {
      auto iter = this->swsr_recv_queues.find(this->get_current_sender_actor().get_actor_id());
      if (iter->second->is_reading_by_reader) {
        iter->second->is_writing_by_sender = false;
      } else {
        delete iter->second;
      }
      this->swsr_recv_queues.erase(iter);
    });
}

void ActorBehaviorX::initialize_actor(ActorSystem& sys, ActorGroup& group) {
  this->ActorBehavior::initialize_actor(sys, group);
  { // a queue for sending messages to self
    this->self_swsr_queue.resize(15);
    this->swsr_send_queues.emplace(this->get_actor_id(), &this->self_swsr_queue);
    this->swsr_recv_queues.emplace(this->get_actor_id(), &this->self_swsr_queue);
  }
}

void ActorBehaviorX::send(const LocalActorHandle& actor, Message* m) {
  auto iter = this->swsr_send_queues.find(actor.local_actor_id);
  if (iter == this->swsr_send_queues.end()) {
    m->set_type(Message::Type::Normal);
    this->ActorBehavior::send(actor, m);
  } else {
    m->set_type(Message::Type::SWSRQueueMessage);
    iter->second->push(m, [&]() {
      this->ActorBehavior::send(actor, DefaultCodes::SWSRMsgQueueNotification);
    }, actor.local_actor_id == this->get_actor_id()
         ? SWSRDeliveryQueueFullStrategy::Resize
         : SWSRDeliveryQueueFullStrategy::Blocking);
  }
}

void ActorBehaviorX::register_swsr_queue(SWSRDeliveryQueue<Message*>* recv_queue) {
  swsr_recv_queues.emplace(this->get_current_sender_actor().get_actor_id(), recv_queue);
}

void ActorBehaviorX::notify_swsr_queue() {
  if (active_recv_queues.empty()) {
    this->ActorBehavior::send(*this, DefaultCodes::SWSRMsgQueueConsumption);
  }
  auto sender_id = this->get_current_sender_actor().get_actor_id();
  active_recv_queues.push_back(swsr_recv_queues.at(sender_id));
  active_recv_queues.back()->is_reading_by_reader = true;
}

void ActorBehaviorX::consume_swsr_recv_queues(MessageHandlers& handlers) {
  for (unsigned i = 0, n = active_recv_queues.size(); i < n; i++) {
    bool empty = false;
    auto& recv_queue = active_recv_queues[i];
    recv_queue->pop_some([&](Message* m) {
      Message* old_current_message = this->current_message;
      this->current_message = m;
      try {
        handlers.process(*m);
      } catch (...) {
        std::throw_with_nested(ZAFException(
          "Exception caught when processing a message with code ", m->get_code()
        ));
      }
      if (this->current_message) {
        delete this->current_message;
      }
      this->current_message = old_current_message;
      if (!this->is_activated()) {
        recv_queue->stop_pop_some();
      }
    }, [&]() {
      empty = true;
    });
    if (empty) {
      if (recv_queue->is_writing_by_sender) {
        recv_queue->is_reading_by_reader = false;
      } else {
        delete recv_queue;
      }
      active_recv_queues[i] = active_recv_queues.back();
      active_recv_queues.pop_back();
      i--;
      n--;
    }
  }
}

ActorBehaviorX::~ActorBehaviorX() {
  for (auto& id2queue : swsr_send_queues) {
    this->ActorBehavior::send(LocalActorHandle{id2queue.first}, DefaultCodes::SWSRMsgQueueTermination);
  }
  swsr_send_queues.clear();
}

void ActorBehaviorX::setup_swsr_connection(const Actor& x) {
  x.visit(overloaded {
    [&](const LocalActorHandle& r) {
      this->setup_swsr_connection(r);
    },
    [&](const RemoteActorHandle& r) {
      this->setup_swsr_connection(LocalActorHandle{r.net_sender_info->id});
    }
  });
}

void ActorBehaviorX::setup_swsr_connection(const LocalActorHandle& actor) {
  auto& swsr_queue = this->swsr_send_queues[actor.local_actor_id];
  if (swsr_queue != nullptr) {
    return;
  }
  swsr_queue = new SWSRDeliveryQueue<Message*>();
  // initialization for a new swsr queue
  swsr_queue->resize(15);
  // send the queue from sender to receiver
  this->ActorBehavior::send(actor, DefaultCodes::SWSRMsgQueueRegistration, swsr_queue);
}
} // namespace zaf
