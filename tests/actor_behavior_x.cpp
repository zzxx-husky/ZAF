#include "zaf/actor_behavior_x.hpp"
#include "zaf/actor_system.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(ActorBehaviorX, ToSelf) {
  ActorSystem actor_system;

  ActorBehaviorX a;
  a.initialize_actor(actor_system, actor_system);

  a.send(a, 0, std::string("ToSelf"));
  a.receive({
    Code{0} - [&](const std::string& x) {
      EXPECT_EQ(x, "ToSelf");
      a.reply(1, std::string("End"));
    },
    Code{1} - [&](const std::string& x) {
      EXPECT_EQ(x, "End");
      a.deactivate();
    }
  });
}

GTEST_TEST(ActorBehaviorX, X2X2X) {
  ActorSystem actor_system;

  ActorBehaviorX a;
  a.initialize_actor(actor_system, actor_system);

  ActorBehaviorX b;
  b.initialize_actor(actor_system, actor_system);
  Actor xb = b.get_local_actor_handle();

  a.send(xb, 0, std::string("X2X"));
  b.receive({
    Code{0} - [&](const std::string& x) {
      EXPECT_EQ(x, "X2X");
      b.reply(1, std::string("X2X2X"));
      b.deactivate();
    }
  });
  a.receive({
    Code{1} - [&](const std::string& x) {
      EXPECT_EQ(x, "X2X2X");
      a.deactivate();
    }
  });
}

GTEST_TEST(ActorBehaviorX, A2X2A) {
  ActorSystem actor_system;

  ActorBehavior a;
  a.initialize_actor(actor_system, actor_system);

  ActorBehaviorX b;
  b.initialize_actor(actor_system, actor_system);
  Actor xb = b.get_local_actor_handle();

  a.send(xb, 0, std::string("A2X"));
  b.receive({
    Code{0} - [&](const std::string& x) {
      EXPECT_EQ(x, "A2X");
      b.reply(1, std::string("A2X2A"));
      b.deactivate();
    }
  });
  a.receive_once({
    Code{1} - [&](const std::string& x) {
      EXPECT_EQ(x, "A2X2A");
    }
  });
}

GTEST_TEST(ActorBehaviorX, X2A2X) {
  ActorSystem actor_system;

  ActorBehaviorX a;
  a.initialize_actor(actor_system, actor_system);

  ActorBehavior b;
  b.initialize_actor(actor_system, actor_system);
  Actor xb = b.get_local_actor_handle();

  a.send(xb, 0, std::string("A2X"));
  b.receive_once({
    Code{0} - [&](const std::string& x) {
      EXPECT_EQ(x, "A2X");
      b.reply(1, std::string("A2X2A"));
    }
  });
  a.receive({
    Code{1} - [&](const std::string& x) {
      EXPECT_EQ(x, "A2X2A");
      a.deactivate();
    }
  });
}
} // namespace zaf
