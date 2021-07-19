#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace zaf {
class ActorGroup {
public:
  void await_all_actors_done();

  void inc_num_alive_actors();

  void dec_num_alive_actors();

private:
  std::atomic<size_t> num_alive_actors{0};
  std::condition_variable all_actors_done_cv;
  std::mutex all_actors_done_mutex;
};
} // namespace zaf
