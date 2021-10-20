#include "zaf/actor_behavior_x.hpp"

namespace zaf {
ActorBehaviorX::ActorBehaviorX() {
  ActorBehavior::inner_handlers.add_handlers(
    DefaultCodes::SWSRMsgQueueRegistration - [&](SWSRDeliveryQueue<Message*>* queue) {
      this->register_swsr_queue(queue);
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

void ActorBehaviorX::send(const LocalActorHandle& actor, Message* m) {
  auto& swsr_queue = this->swsr_send_queues[actor.local_actor_id];
  if (swsr_queue == nullptr) {
    swsr_queue = new SWSRDeliveryQueue<Message*>();
    // initialization for a new swsr queue
    swsr_queue->resize(15);
    // send the queue from sender to receiver
    this->ActorBehavior::send(actor, DefaultCodes::SWSRMsgQueueRegistration, swsr_queue);
  }
  swsr_queue->push(m, [&]() {
    this->ActorBehavior::send(actor, DefaultCodes::SWSRMsgQueueNotification);
  }, actor.local_actor_id == this->get_actor_id()
       ? SWSRDeliveryQueueFullStrategy::Resize
       : SWSRDeliveryQueueFullStrategy::Blocking);
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
    active_recv_queues[i]->pop_some([&](Message* m) {
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
    }, [&]() {
      empty = true;
    });
    if (empty) {
      if (active_recv_queues[i]->is_writing_by_sender) {
        active_recv_queues[i]->is_reading_by_reader = false;
      } else {
        delete active_recv_queues[i];
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
} // namespace zaf
