#include "zaf/zaf.hpp"

// define an actor by inherting zaf::ActorBehavior
class X : public zaf::ActorBehavior {
public:
  zaf::MessageHandlers behavior() override {
    return {
      zaf::Code{1} - [&](const std::string& hello) {
        LOG(INFO) << "Expected hello, received " << hello;
        this->send(this->get_current_sender_actor(), 0, std::string("world"));
      },
      zaf::Code{2} - [&](const std::string& hw) {
        LOG(INFO) << "Expected hello world, received " << hw;
        this->deactivate();
      }
    };
  }
};

class Y : public zaf::ActorBehavior {
public:
  Y(zaf::Actor x): x(x) {}

  void start() override {
    this->send(x, 1, std::string("hello"));
  }

  zaf::MessageHandlers behavior() override {
    return {
      zaf::Code{0} - [&](const std::string& world) {
        LOG(INFO) << "Expected world, received " << world;
        this->reply(2, std::string("hello world"));
        this->deactivate();
      }
    };
  }

  zaf::Actor x;
};

int main(int argc, char** argv) {
  zaf::ActorSystem actor_system;
  zaf::ActorEngine engine{actor_system, 1};
  auto x = engine.spawn<X>();
  auto y = engine.spawn<Y>(x);
  // engine.await_all_actors_done();
  // engine.terminate();
  // actor_system.await_all_actors_done();
}

