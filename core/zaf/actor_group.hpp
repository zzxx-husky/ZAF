#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "actor.hpp"
#include "actor_behavior.hpp"
#include "scoped_actor.hpp"

namespace zaf {
class ActorGroup {
public:
  void await_alive_actors_done();
  void await_all_actors_done();
  void inc_num_alive_actors();
  void dec_num_alive_actors();
  void inc_num_detached_actors();
  void dec_num_detached_actors();

  template<typename ActorClass, typename ... ArgT>
  Actor spawn(ArgT&& ... args) {
    return spawn(new ActorClass(std::forward<ArgT>(args)...));
  }

  void add_terminator(std::function<void(ScopedActor<ActorBehavior>&)>);
  void flush_terminators();

  virtual Actor spawn(ActorBehavior* new_actor) = 0;

  inline ScopedActor<ActorBehavior> create_scoped_actor() {
    return create_scoped_actor<ActorBehavior>();
  }

  template<typename ActorType>
  inline ScopedActor<ActorType> create_scoped_actor() {
    auto new_actor = new ActorType();
    init_scoped_actor(*new_actor);
    return {new_actor};
  }

protected:
  virtual void init_scoped_actor(ActorBehavior&) = 0;

private:
  std::atomic<size_t> num_alive_actors{0};
  std::atomic<size_t> num_detached_actors{0}; // must be not larger than `num_alive_actors`
  std::condition_variable all_actors_done_cv, alive_actors_done_cv;
  std::mutex await_actors_done_mtx;

  std::vector<std::function<void(ScopedActor<ActorBehavior>&)>> terminators;
  std::mutex terminator_mutex;
};
} // namespace zaf
