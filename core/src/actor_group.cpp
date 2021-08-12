#include "zaf/actor_group.hpp"

namespace zaf {
void ActorGroup::await_all_actors_done() {
  while (true) {
    this->await_alive_actors_done();
    flush_terminators();
    if (num_alive_actors.load(std::memory_order::memory_order_relaxed) == 0) {
      break;
    }
    std::unique_lock<std::mutex> lock(await_actors_done_mtx);
    all_actors_done_cv.wait(lock, [&]() {
      auto num_alive = num_alive_actors.load(std::memory_order::memory_order_relaxed);
      return num_alive == 0 ||
        num_alive != num_detached_actors.load(std::memory_order::memory_order_relaxed);
    });
  }
}

void ActorGroup::await_alive_actors_done() {
  if (num_alive_actors.load(std::memory_order::memory_order_relaxed) ==
      num_detached_actors.load(std::memory_order::memory_order_relaxed)) {
    return;
  }
  std::unique_lock<std::mutex> lock(await_actors_done_mtx);
  alive_actors_done_cv.wait(lock, [&]() {
    return num_alive_actors.load(std::memory_order::memory_order_relaxed) ==
      num_detached_actors.load(std::memory_order::memory_order_relaxed);
  });
}

void ActorGroup::inc_num_alive_actors() {
  num_alive_actors.fetch_add(1, std::memory_order::memory_order_relaxed);
}

void ActorGroup::inc_num_detached_actors() {
  auto num_detached = num_detached_actors.fetch_add(1, std::memory_order::memory_order_relaxed) + 1;
  if (num_detached == num_alive_actors.load(std::memory_order::memory_order_relaxed)) {
    this->alive_actors_done_cv.notify_one();
  }
}

void ActorGroup::dec_num_alive_actors() {
  auto num_alive = num_alive_actors.fetch_sub(1, std::memory_order::memory_order_relaxed) - 1;
  if (num_alive == num_detached_actors.load(std::memory_order::memory_order_relaxed)) {
    this->alive_actors_done_cv.notify_one();
  }
  if (num_alive == 0) {
    this->all_actors_done_cv.notify_one();
  }
}

void ActorGroup::dec_num_detached_actors() {
  num_detached_actors.fetch_sub(1, std::memory_order::memory_order_relaxed);
}

void ActorGroup::add_terminator(std::function<void(ScopedActor<ActorBehavior>&)> t) {
  terminator_mutex.lock();
  terminators.push_back(t);
  terminator_mutex.unlock();
}

void ActorGroup::flush_terminators() {
  terminator_mutex.lock();
  auto ts = std::move(terminators);
  terminator_mutex.unlock();
  if (!ts.empty()) {
    auto actor = this->create_scoped_actor();
    for (auto& t : ts) {
      t(actor);
    }
  }
}
} // namespace zaf
