#include "zaf/actor_behavior.hpp"
#include "zaf/actor_system.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(ActorBehavior, Basic) {
  ActorSystem actor_system;
  {
    ActorBehavior actor1;
    actor1.initialize_actor(actor_system, actor_system);

    ActorBehavior actor2;
    actor2.initialize_actor(actor_system, actor_system);

    actor1.send({actor2.get_actor_id()}, 0, std::string("Hello World"));
    actor2.receive({
      Code{0} - [](const std::string& hw) {
        EXPECT_EQ(hw, "Hello World");
      }
    });
  }
}
} // namespace zaf
