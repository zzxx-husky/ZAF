#include "zaf/actor_engine.hpp"

#include "zmq.hpp"

namespace zaf {

ActorEngine::ActorEngine(ActorSystem& actor_system, size_t num_executors) {
  initialize(actor_system, num_executors);
}

void ActorEngine::initialize(ActorSystem& actor_system, size_t num_executors) {
  if (this->num_executors != 0) {
    throw std::runtime_error("Attempt to initialize an active ActorEngine.");
  }
  this->num_executors = num_executors;
  this->forwarder = actor_system.create_scoped_actor();
  this->executors.resize(num_executors);
  for (auto& e : executors) {
    e = actor_system.spawn<Executor>(*this);
  }
}

void ActorEngine::terminate() {
  for (auto& e : executors) {
    forwarder->send(e, Executor::Termination);
  }
  await_all_actors_done();
  this->forwarder = nullptr;
  executors.clear();
  num_executors = 0;
}

ActorEngine::~ActorEngine() {
  await_all_actors_done();
  for (auto& e : executors) {
    forwarder->send(e, Executor::Termination);
  }
  this->forwarder = nullptr;
  executors.clear();
  num_executors = 0;
}

ActorEngine::Executor::Executor(ActorEngine& engine):
  engine(engine) {
}

MessageHandlers ActorEngine::Executor::behavior() {
  return {
    NewActor - [=](ActorBehavior* new_actor) {
      new_actor->activate();
      new_actor->start();
      if (new_actor->is_activated()) {
        listen_to_actor(new_actor);
      }
    },
    Termination - [=]() {
      this->deactivate();
    }
  };
}

void ActorEngine::Executor::listen_to_actor(ActorBehavior* new_actor) {
  actors.push_back(new_actor);
  handlers.emplace_back(new_actor->behavior());
  poll_items.emplace_back(zmq::pollitem_t{
    new_actor->get_recv_socket().handle(), 0, ZMQ_POLLIN, 0
  });
  ++num_poll_items;
}

void ActorEngine::Executor::launch() {
  // start
  listen_to_actor(this);
  // receive
  this->activate();
  while (this->is_activated()) {
    // block until receive a message
    int npoll = zmq::poll(poll_items);
    if (npoll == 0) {
      continue;
    }
    for (size_t i = 0; i < num_poll_items; i++) {
      if (poll_items[i].revents & ZMQ_POLLIN) {
        actors[i]->receive_once(handlers[i]);
        if (!actors[i]->is_activated() && i != 0) {
          actors[i]->stop();
          engine.dec_num_alive_actors();
          delete actors[i];
          actors[i] = actors.back();
          actors.pop_back();
          handlers[i] = std::move(handlers.back());
          handlers.pop_back();
          poll_items[i] = poll_items.back();
          poll_items.pop_back();
          num_poll_items--;
          i--;
        }
      }
    }
  }
  // stop
  for (size_t i = 1, n = actors.size(); i < n; i++) {
    actors[i]->stop();
    engine.dec_num_alive_actors();
    delete actors[i];
  }
  actors.clear();
  poll_items.clear();
  handlers.clear();
  num_poll_items = 0;
}
} // namespace zaf
