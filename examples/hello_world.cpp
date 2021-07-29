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

int main(int argc, char** argv) {
  zaf::ActorSystem actor_system;
  auto x = actor_system.spawn<X>();
  auto y = actor_system.spawn([x](zaf::ActorBehavior& self) {
    self.send(x, 1, std::string("hello"));
    self.receive({
      zaf::Code{0} - [&](const std::string& world) {
        LOG(INFO) << "Expected world, received " << world;
        self.send(self.get_current_sender_actor(), 2, std::string("hello world"));
        self.deactivate();
      }
    });
  });
  // actor_system.await_all_actors_done();
}
