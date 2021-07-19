#include "zaf/actor_group.hpp"

namespace zaf {
void ActorGroup::await_all_actors_done() {
  if (num_alive_actors.load(std::memory_order::memory_order_relaxed) == 0) {
    return;
  }
  std::unique_lock<std::mutex> lock(all_actors_done_mutex);
  all_actors_done_cv.wait(lock, [&]() {
    return num_alive_actors.load(std::memory_order::memory_order_relaxed) == 0;
  });
}

void ActorGroup::inc_num_alive_actors() {
  num_alive_actors.fetch_add(1, std::memory_order::memory_order_relaxed);
}

void ActorGroup::dec_num_alive_actors() {
  if (num_alive_actors.fetch_sub(1, std::memory_order::memory_order_relaxed) == 1) {
    this->all_actors_done_cv.notify_one();
  }
}
} // namespace zaf

