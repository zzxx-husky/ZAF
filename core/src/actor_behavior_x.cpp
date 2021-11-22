#include "zaf/actor_behavior_x.hpp"

namespace zaf {
ActorBehaviorX::ActorBehaviorX() {
  ActorBehavior::inner_handlers.add_handlers(
    DefaultCodes::SWSRMsgQueueRegistration -
    [&](std::shared_ptr<SWSRDeliveryQueue<Message*>>& queue) {
      this->register_swsr_queue(queue);
    },
    DefaultCodes::SWSRMsgQueueNotification - [&]() {
      this->notify_swsr_queue();
    },
    DefaultCodes::SWSRMsgQueueConsumption - [&]() {
      this->consume_swsr_recv_queues(ActorBehavior::inner_handlers.get_child_handlers());
      this->post_swsr_consumption();
      if (!this->active_recv_queues.empty()) {
        this->ActorBehavior::send(*this, DefaultCodes::SWSRMsgQueueConsumption);
      }
    });
}

void ActorBehaviorX::initialize_actor(ActorSystem& sys, ActorGroup& group) {
  this->ActorBehavior::initialize_actor(sys, group);
  { // a queue for sending messages to self
    this->self_swsr_queue = std::make_shared<SWSRDeliveryQueue<Message*>>();
    this->self_swsr_queue->resize(15);
    this->self_swsr_queue->destructor = [queue = this->self_swsr_queue.get()]() {
      queue->pop_some([](Message* m) {
        delete m;
      }, queue->size());
    };
    this->swsr_send_queues.emplace(this->get_actor_id(), this->self_swsr_queue);
    this->swsr_recv_queues.emplace(this->get_actor_id(), this->self_swsr_queue);
  }
}

LocalActorHandle ActorBehaviorX::get_local_actor_handle() const {
  return {this->get_actor_id(), true};
}

void ActorBehaviorX::send(const LocalActorHandle& actor, Message* m) {
  if (!actor) {
    return;
  }
  if (!actor.use_swsr_msg_delivery) {
    this->ActorBehavior::send(actor, m);
    return;
  }
  auto& send_queue = this->swsr_send_queues[actor.local_actor_id];
  if (send_queue == nullptr) {
    this->setup_swsr_connection(actor);
  }
  send_queue->push(m, [&]() {
    this->ActorBehavior::send(actor, DefaultCodes::SWSRMsgQueueNotification);
  }, actor.local_actor_id == this->get_actor_id()
       ? SWSRDeliveryQueueFullStrategy::Resize
       : SWSRDeliveryQueueFullStrategy::Blocking);
}

std::string ActorBehaviorX::get_name() const {
  return to_string("ZAF/AX", this->get_actor_id());
}

void ActorBehaviorX::register_swsr_queue(std::shared_ptr<SWSRDeliveryQueue<Message*>>& recv_queue) {
  swsr_recv_queues.emplace(this->get_current_sender_actor().get_actor_id(), std::move(recv_queue));
}

void ActorBehaviorX::notify_swsr_queue() {
  if (active_recv_queues.empty()) {
    this->ActorBehavior::send(*this, DefaultCodes::SWSRMsgQueueConsumption);
  }
  auto sender_id = this->get_current_sender_actor().get_actor_id();
  active_recv_queues.emplace_back(sender_id, swsr_recv_queues.at(sender_id).get());
}

void ActorBehaviorX::consume_swsr_recv_queues(MessageHandlers& handlers) {
  Message* old_current_message = this->current_message;
  this->current_message = nullptr;
  for (unsigned i = 0, n = active_recv_queues.size(); i < n; i++) {
    bool empty = false;
    auto recv_queue = active_recv_queues[i].second;
    recv_queue->pop_some([&](Message* m) {
      this->current_message = m;
      try {
        handlers.process(*m);
      } catch (...) {
        std::throw_with_nested(ZAFException(
          "Exception caught when processing a message with code ",
          m->get_body().get_code(), " (", std::hex, m->get_body().get_code(), ")."));
      }
      if (this->current_message) {
        delete this->current_message;
        this->current_message = nullptr;
      }
      if (!this->is_activated()) {
        recv_queue->stop_pop_some();
      }
    }, [&]() {
      empty = true;
    });
    if (empty) {
      if (!recv_queue->is_writing_by_sender) {
        this->swsr_recv_queues.erase(active_recv_queues[i].first);
      }
      active_recv_queues[i] = active_recv_queues.back();
      active_recv_queues.pop_back();
      i--;
      n--;
    }
  }
  this->current_message = old_current_message;
}

void ActorBehaviorX::post_swsr_consumption() {}

ActorBehaviorX::~ActorBehaviorX() {
  for (auto& id2queue : swsr_send_queues) {
    id2queue.second->is_writing_by_sender = false;
  }
  swsr_send_queues.clear();
  swsr_recv_queues.clear();
  self_swsr_queue = nullptr;
}

void ActorBehaviorX::setup_swsr_connection(const Actor& x) {
  if (!x) {
    return;
  }
  x.visit(overloaded {
    [&](const LocalActorHandle& r) {
      this->setup_swsr_connection(r);
    },
    [&](const RemoteActorHandle& r) {
      this->setup_swsr_connection(r.net_sender_info->net_sender);
    }
  });
}

void ActorBehaviorX::setup_swsr_connection(const LocalActorHandle& actor) {
  auto& swsr_queue = this->swsr_send_queues[actor.local_actor_id];
  if (swsr_queue != nullptr) {
    return;
  }
  swsr_queue = std::make_shared<SWSRDeliveryQueue<Message*>>();
  // initialization for a new swsr queue
  swsr_queue->resize(15);
  swsr_queue->destructor = [queue = swsr_queue.get()]() {
    queue->pop_some([](Message* m) {
      delete m;
    }, queue->size());
  };
  // send the queue from sender to receiver
  this->ActorBehavior::send(actor, DefaultCodes::SWSRMsgQueueRegistration, swsr_queue);
}
} // namespace zaf
