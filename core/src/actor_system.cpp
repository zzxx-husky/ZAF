#include "zaf/actor_behavior.hpp"
#include "zaf/actor_system.hpp"
#include "zaf/scoped_actor.hpp"

namespace zaf {
Actor ActorSystem::spawn(ActorBehavior* new_actor) {
  new_actor->initialize_actor(*this, *this);
  std::thread([new_actor]() mutable {
    try {
      new_actor->launch();
    } catch (const std::exception& e) {
      std::cerr << "Exception caught when running an actor at " << __PRETTY_FUNCTION__ << std::endl;
      print_exception(e);
    } catch (...) {
      std::cerr << "Unknown exception caught when running an actor at " << __PRETTY_FUNCTION__ << std::endl;
    }
    delete new_actor;
  }).detach();
  return {new_actor->get_local_actor_handle()};
}

void ActorSystem::init_scoped_actor(ActorBehavior& new_actor) {
  new_actor.initialize_actor(*this, *this);
}

zmq::context_t& ActorSystem::get_zmq_context() {
  return zmq_context;
}

ActorIdType ActorSystem::get_next_available_actor_id() {
  return next_available_actor_id.fetch_add(1, std::memory_order_relaxed) % MaxActorId;
}

ActorSystem::~ActorSystem() {
  this->await_all_actors_done();
  zmq_context.close();
}

void ActorSystem::set_identifier(const std::string& identifier) {
  if (num_alive_actors.load(std::memory_order_relaxed) == 0) {
    throw ZAFException("ActorSystem's identifier cannot be updated if there is any alive actor.");
  }
  this->identifier = identifier;
}

const std::string& ActorSystem::get_identifier() const {
  return this->identifier;
}
} // namespace zaf
