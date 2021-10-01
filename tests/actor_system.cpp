#include "zaf/actor_system.hpp"
#include "zaf/thread_based_actor_behavior.hpp"

#include "gtest/gtest.h"
#include "glog/logging.h"

namespace zaf {
GTEST_TEST(ActorSystem, Basic) {
  ActorSystem actor_system;
}

GTEST_TEST(ActorSystem, ScopedActor) {
  ActorSystem actor_system;
  int i = 0;
  {
    auto s = actor_system.create_scoped_actor();
    s->send(*s, 0);
    s->receive_once({
      Code{0} - [&]() {
        i++;
      }
    });
  }
  EXPECT_EQ(i, 1);
}

GTEST_TEST(ActorSystem, LambdaBasedActorCreation) {
  int z = 0;
  {
    ActorSystem actor_system;
    auto a = actor_system.spawn([&](ActorBehavior& x) {
      x.receive_once({
        Code{0} - [&]() { x.reply(1); }
      });
    });
    actor_system.spawn([&](ThreadBasedActorBehavior& x) {
      LOG(INFO) << "Delay send for 1 second";
      x.delayed_send(std::chrono::seconds{1}, a, 0);
      x.receive_once({
        Code{1} - [&]() { z++; }
      });
    });
    actor_system.add_terminator([&](auto&) {
      EXPECT_EQ(z, 1);
    });
  }
}

GTEST_TEST(ActorSystem, Terminator) {
  int z = 1;
  {
    ActorSystem actor_system;
    actor_system.spawn([&](ActorBehavior& x) {
      x.get_actor_system().inc_num_detached_actors();
      x.get_actor_system().add_terminator([&](auto& s) {
        s->send(x, 0);
      });
      x.receive({
        Code{0} - [&]() {
          x.get_actor_system().dec_num_detached_actors();
          x.deactivate();
        }
      });
    });
    actor_system.spawn([&](ThreadBasedActorBehavior& x) {
      LOG(INFO) << "Delay send for 1 second";
      x.delayed_send(std::chrono::seconds{1}, x, 1);
      x.receive_once({
        Code{1} - [&]() { z--; }
      });
    });
    actor_system.add_terminator([&](auto&) {
      EXPECT_EQ(z, 0);
    });
  }
}
} // namespace zaf
