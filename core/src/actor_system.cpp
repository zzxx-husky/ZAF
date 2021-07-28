#include "zaf/actor_behavior.hpp"
#include "zaf/actor_system.hpp"
#include "zaf/scoped_actor.hpp"

namespace zaf {
Actor ActorSystem::spawn(ActorBehavior* new_actor) {
  new_actor->initialize_actor(*this, *this);
  // only after initialize_actor we get the actor id.
  auto new_actor_id = new_actor->get_actor_id();
  std::thread([new_actor]() mutable {
    new_actor->launch();
    delete new_actor;
  }).detach();
  return {new_actor_id};
}

ScopedActor ActorSystem::create_scoped_actor() {
  auto new_actor = new ActorBehavior();
  new_actor->initialize_actor(*this, *this);
  return {new_actor};
}

zmq::context_t& ActorSystem::get_zmq_context() {
  return zmq_context;
}

ActorIdType ActorSystem::get_next_available_actor_id() {
  return next_available_actor_id.fetch_add(1, std::memory_order::memory_order_relaxed) % MaxActorId;
}

ActorSystem::~ActorSystem() {
  this->await_all_actors_done();
  zmq_context.close();
}
} // namespace zaf
