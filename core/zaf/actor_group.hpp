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
  void await_all_actors_done();

  void inc_num_alive_actors();

  void dec_num_alive_actors();

  template<typename ActorClass, typename ... ArgT>
  Actor spawn(ArgT&& ... args) {
    return spawn(new ActorClass(std::forward<ArgT>(args)...));
  }

  virtual Actor spawn(ActorBehavior* new_actor) = 0;
  virtual ScopedActor create_scoped_actor() = 0;

private:
  std::atomic<size_t> num_alive_actors{0};
  std::condition_variable all_actors_done_cv;
  std::mutex all_actors_done_mutex;
};
} // namespace zaf
