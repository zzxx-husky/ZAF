#pragma once

#include <exception>
#include <stdexcept>

#include "actor.hpp"
#include "actor_behavior.hpp"
#include "actor_group.hpp"
#include "actor_system.hpp"

namespace zaf {
class ActorEngine : public ActorGroup {
public:
  ActorEngine() = default;

  ActorEngine(ActorSystem& actor_system, size_t num_executors);

  void initialize(ActorSystem& actor_system, size_t num_executors);

  template<typename ActorClass, typename ... ArgT>
  Actor spawn(ArgT&& ... args) {
    auto new_actor = new ActorClass(std::forward<ArgT>(args)...);
    new_actor->initialize_actor(forwarder->get_actor_system());
    auto new_actor_id = new_actor->get_actor_id();
    this->inc_num_alive_actors();
    forwarder->send(executors[next_executor++ % num_executors], Executor::NewActor, new_actor);
    return {new_actor_id};
  }

  void terminate();

  ~ActorEngine();

private:
  class Executor : public ActorBehavior {
  public:
    inline const static Code NewActor{0};
    inline const static Code Termination{1};

    Executor(ActorEngine& engine);

    MessageHandlers behavior() override;

    void listen_to_actor(ActorBehavior* new_actor);

    void launch() override;

  private:
    ActorEngine& engine;
    std::vector<ActorBehavior*> actors;       // actors[0] will be `this`
    std::vector<MessageHandlers> handlers;    // handlers[0] will be `this->behave()`
    std::vector<zmq::pollitem_t> poll_items;  // poll_items[0] will be the poll item of this
    size_t num_poll_items = 0;
  };

  size_t num_executors = 0, next_executor = 0;
  std::vector<Actor> executors;
  ScopedActor forwarder;
};
} // namespace zaf
