#include "zaf/actor_behavior.hpp"
#include "zaf/actor_system.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(ActorBehavior, Basic) {
  ActorSystem actor_system;

  ActorBehavior actor1;
  actor1.initialize_actor(actor_system, actor_system);

  ActorBehavior actor2;
  actor2.initialize_actor(actor_system, actor_system);

  actor1.send(actor2, 0, std::string("Hello World"));
  actor2.receive_once({
    Code{0} - [](const std::string& hw) {
      EXPECT_EQ(hw, "Hello World");
    }
  });
}

GTEST_TEST(ActorBehavior, DelayedSendWithReceiveTimeout) {
  ActorSystem actor_system;

  auto a = actor_system.spawn([&](ActorBehavior& self) {
    self.delayed_send(std::chrono::seconds{1}, self, Code{0});
    bool received = false;
    for (int i = 0; i < 4; i++) {
      self.receive_once({
        Code{0} - [&]() {
          received = true;
        }
      }, std::chrono::milliseconds{200});
      EXPECT_FALSE(received);
    }
    self.receive_once({
      Code{0} - [&]() {
        received = true;
      }
    }, std::chrono::milliseconds{210});
    EXPECT_TRUE(received);
  });
}
} // namespace zaf
