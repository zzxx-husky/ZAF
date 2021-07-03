#pragma once

#include <atomic>
#include <condition_variable>
#include <thread>
#include <type_traits>

#include "actor.hpp"
#include "actor_behavior.hpp"
#include "scoped_actor.hpp"

#include "zmq.hpp"

namespace zaf {
class ActorSystem {
public:
  ActorSystem() = default;

  ActorSystem(const ActorSystem&) = delete;

  ActorSystem(ActorSystem&&);

  template<typename ActorClass, typename ... ArgT>
  Actor spawn(ArgT&& ... args) {
    auto new_actor = new ActorClass(std::forward<ArgT>(args) ...);
    new_actor->initialize_actor(*this);
    std::thread new_actor_thread([new_actor]() mutable {
      new_actor->launch();
      delete new_actor;
    });
    new_actor_thread.detach();
    return {new_actor->get_actor_id()};
  }

  ScopedActor create_scoped_actor();

  template<typename Behavior, typename ... ArgT,
    std::enable_if_t<std::is_invocable_v<Behavior, ActorBehavior&, ArgT...>>* = nullptr>
  Actor spawn(Behavior behave, ArgT&& ... args) {
    auto new_actor = new ActorBehavior();
    new_actor->initialize_actor(*this);
    std::thread new_actor_thread([=]() mutable {
      behave(*new_actor, std::forward<ArgT>(args)...);
      delete new_actor;
    });
    new_actor_thread.detach();
    return {new_actor->get_actor_id()};
  }

  zmq::context_t& get_zmq_context();

  ActorIdType get_next_available_actor_id();

  void inc_num_alive_actors();

  void dec_num_alive_actors();

  void await_all_actors_done();

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
