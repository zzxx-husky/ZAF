#include "zaf/zaf.hpp"

class X : public zaf::ActorBehavior {
public:
  X(std::chrono::milliseconds work_duration):
    work_duration(work_duration) {
  }

  void start() override {
    this->send(*this, 0, size_t(0));
  }

  zaf::MessageHandlers behavior() override {
    return {
      zaf::Code{0} - [&](size_t i) {
        std::this_thread::sleep_for(work_duration);
        if (i == 100) {
          this->deactivate();
        } else {
          this->send(*this, 0, i + 1);
        }
      }
    };
  }

  const std::chrono::milliseconds work_duration;
};

int main() {
  zaf::ActorSystem actor_system;

  for (int i = 0; i < 3; i++) {
    zaf::ActorEngine engine{actor_system, 4};

    auto start = std::chrono::system_clock::now();
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        engine.spawn<X>(std::chrono::milliseconds{(j + 1) * 2});
      }
    }
    engine.await_all_actors_done();
    auto end = std::chrono::system_clock::now();
    LOG(INFO) << "Duration w/o load rebalance (trial " << (i + 1) << "): "
      << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";
  }

  for (int i = 0; i < 3; i++) {
    zaf::ActorEngine engine{actor_system, 4};
    engine.set_load_diff_ratio(0.1);
    engine.set_load_rebalance_period(10);

    auto start = std::chrono::system_clock::now();
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        engine.spawn<X>(std::chrono::milliseconds{(j + 1) * 2});
      }
    }
    engine.await_all_actors_done();
    auto end = std::chrono::system_clock::now();
    LOG(INFO) << "Duration w/ load rebalance (trial " << (i + 1) << "): "
      << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";
  }

  LOG(INFO) << "Best duration should be " << (1 + 4) * 4 / 2 * 2 * 100 << "ms";
}
