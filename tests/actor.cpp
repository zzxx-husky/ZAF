#include "zaf/actor_system.hpp"
#include "zaf/net_gate.hpp"
#include "zaf/net_gate_client.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(Actor, Send) {
  int i = 0;
  {
    ActorSystem sys;
    auto a = sys.spawn([&](ActorBehavior& self) {
      self.receive({
        Code{0} - [&]() {
          i++;
          self.send(self, 1);
        },
        Code{1} - [&]() {
          i++;
          self.deactivate();
        }
      });
    });
    auto b = sys.create_scoped_actor();
    b->send(a, 0);
  }
  EXPECT_EQ(i, 2);
}

GTEST_TEST(Actor, LocalActorLocalTransfer) {
  int i = 0;
  {
    ActorSystem sys;
    auto a = sys.spawn([&](ActorBehavior& self) {
      self.receive_once({
        Code{0} - [&]() {
          i++;
        }
      });
    });
    auto b = sys.spawn([&](ActorBehavior& self) {
      self.receive_once({
        Code{1} - [&](Actor x) {
          self.send(x, 0);
        }
      });
    });
    auto c = sys.create_scoped_actor();
    c->send(b, 1, a);
  }
  EXPECT_EQ(i, 1);
}

GTEST_TEST(Actor, RemoteActorLocalTransfer) {
  int i = 0;
  std::thread machine_a([&]() {
    ActorSystem sys;
    NetGate gate{sys, "127.0.0.1", 45678};
    NetGateClient client{gate.actor()};
    auto sender = sys.create_scoped_actor();
    client.register_actor(*sender, "A", sys.spawn([&](ActorBehavior& self) {
      self.receive_once({
        Code{0} - [&]() {
          i++;
        }
      });
    }));
  });
  std::thread machine_b([&]() {
    ActorSystem sys;
    NetGate gate{sys, "127.0.0.1", 34567};
    auto b = sys.spawn([&](ActorBehavior& self) {
      self.receive_once({
        Code{1} - [&](Actor x) {
          self.send(x, 0);
        }
      });
    });
    NetGateClient client{gate.actor()};
    auto c = sys.create_scoped_actor();
    client.lookup_actor(*c, "127.0.0.1:45678", "A");
    c->receive_once({
      client.on_lookup_actor_reply([&](std::string&, std::string&, Actor a) {
        c->send(b, 1, a);
      })
    });
  });
  machine_a.join();
  machine_b.join();
  EXPECT_EQ(i, 1);
}

// GTEST_TEST(Actor, RemoteActorTransferToOriginalActorSystem) 
// GTEST_TEST(Actor, RemoteActorTransferToAThirdActorSystem) 
} // namespace zaf
