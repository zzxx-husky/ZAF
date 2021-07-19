#pragma once

#include <thread>
#include <type_traits>

#include "actor.hpp"
#include "actor_behavior.hpp"
#include "actor_group.hpp"
#include "scoped_actor.hpp"

#include "zmq.hpp"

namespace zaf {
class ActorSystem : public ActorGroup {
public:
  ActorSystem() = default;

  ActorSystem(const ActorSystem&) = delete;
  ActorSystem& operator=(const ActorSystem&) = delete;

  // The reason ActorSystem is not movable is becuz ActorBehavior keeps a reference to ActorSystem.
  // The reference in ActorBehavior will not be updated if ActorSystem is moved.
  // ActorSystem(ActorSystem&&);
  // ActorSystem& operator=(ActorSystem&&);

  template<typename ActorClass, typename ... ArgT>
  Actor spawn(ArgT&& ... args) {
    auto new_actor = new ActorClass(std::forward<ArgT>(args) ...);
    new_actor->initialize_actor(*this);
    // only after initialize_actor we get the actor id.
    auto new_actor_id = new_actor->get_actor_id();
    std::thread([new_actor]() mutable {
      new_actor->launch();
      delete new_actor;
    }).detach();
    return {new_actor_id};
  }

  ScopedActor create_scoped_actor();

  template<typename Runnable,
    std::enable_if_t<std::is_invocable_v<Runnable, ActorBehavior&>>* = nullptr>
  Actor spawn(Runnable&& runnable) {
    auto new_actor = new ActorBehavior();
    new_actor->initialize_actor(*this);
    // only after initialize_actor we get the actor id.
    auto new_actor_id = new_actor->get_actor_id();
    std::thread([run = std::forward<Runnable>(runnable), new_actor]() mutable {
      run(*new_actor);
      delete new_actor;
    }).detach();
    return {new_actor_id};
  }

  zmq::context_t& get_zmq_context();

  ActorIdType get_next_available_actor_id();

  ~ActorSystem();

private:
  std::atomic<size_t> num_alive_actors{0};
  std::condition_variable all_actors_done_cv;
  std::mutex all_actors_done_mutex;

  // Available actor id starts from 1. Actor id 0 means null actor.
  std::atomic<ActorIdType> next_available_actor_id{1};

  zmq::context_t zmq_context;
};
} // namespace zaf
