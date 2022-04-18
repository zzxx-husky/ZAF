#include "zaf/actor_behavior.hpp"
#include "zaf/actor_system.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(Request, ByReply) {
  ActorSystem actor_system;

  ActorBehavior actor1;
  actor1.initialize_actor(actor_system, actor_system);

  ActorBehavior actor2;
  actor2.initialize_actor(actor_system, actor_system);

  auto req = actor1.request(actor2, 0, std::string("Hello World"));

  actor2.receive_once({
    Code{0} - [&](std::string hw) {
      EXPECT_EQ(hw, "Hello World");
      std::reverse(hw.begin(), hw.end());
      actor2.reply(1, hw);
    }
  });

  req.on_reply({
    Code{1} - [&](const std::string r) {
      EXPECT_EQ(r, "dlroW olleH");
    }
  });
}

GTEST_TEST(Request, ByResponse) {
  ActorSystem actor_system;

  ActorBehavior actor1;
  actor1.initialize_actor(actor_system, actor_system);

  ActorBehavior actor2;
  actor2.initialize_actor(actor_system, actor_system);

  auto req = actor1.request(actor2, 0, std::string("Hello World"));

  actor2.receive_once({
    Code{0} - [&](std::string hw) {
      EXPECT_EQ(hw, "Hello World");
      actor2.send(actor2.get_current_sender_actor(), 2, hw);
      std::reverse(hw.begin(), hw.end());
      actor2.send(actor2.get_current_sender_actor(), DefaultCodes::Response,
        get_request_id(actor2.get_current_message()),
        std::unique_ptr<MessageBody>(new_message(Code{1}, hw)));
    }
  });

  req.on_reply({
    Code{1} - [&](const std::string& r) {
      EXPECT_EQ(r, "dlroW olleH");
    }
  });

  actor1.receive_once({
    Code{2} - [&](const std::string& hw) {
      EXPECT_EQ(hw, "Hello World");
    }
  });
}

GTEST_TEST(Request, ByReply2) {
  ActorSystem actor_system;

  auto a = actor_system.spawn([&](ActorBehavior& self) {
    self.receive({
      Code{0} - [&](int v) { self.reply(0, v * 2); },
      Code{1} - [&](int v) { self.reply(1, v / 2); },
      Code{2} - [&](int v) { self.reply(2, v + 2); },
      Code{3} - [&]() {
        self.deactivate();
      },
      Code{4} - [&](int v) { self.reply(4, v - 2); },
    });
  });

  actor_system.spawn([&](ActorBehavior& self) {
    self.send(a, 4, 12341);
    self.request(a, 0, 1234).on_reply({
      Code{0} - [&](int v) {
        EXPECT_EQ(v, 1234 * 2);
        self.send(a, 4, 12342);
        self.request(a, 1, 1234).on_reply({
          Code{1} - [&](int v) {
            EXPECT_EQ(v, 1234 / 2);
            self.send(a, 4, 12343);
            self.request(a, 2, 1234).on_reply({
              Code{2} - [&](int v) {
                EXPECT_EQ(v, 1234 + 2);
                self.send(a, 4, 12344);
                self.reply(3);
              }
            });
          }
        });
      }
    });
    int i = 0;
    self.receive({
      Code{4} - [&](int v) {
        EXPECT_EQ(v, 1234 * 10 + (++i) - 2);
        if (i == 4) {
          self.deactivate();
        }
      }
    });
  });
}

GTEST_TEST(Request, ByResponse2) {
  ActorSystem actor_system;

  auto a = actor_system.spawn([&](ActorBehavior& self) {
    CountPointer<Message> requester;
    Actor target;
    self.receive({
      Code{0} - [&]() {
        target = self.get_current_sender_actor(); 
        if (requester) {
          self.send(requester->get_sender(), DefaultCodes::Response,
            get_request_id(*requester),
            std::unique_ptr<MessageBody>(new_message(Code{1}, target)));
          self.deactivate();
        }
      },
      Code{1} - [&]() {
        if (target) {
          self.send(self.get_current_sender_actor(), DefaultCodes::Response,
            get_request_id(self.get_current_message()),
            std::unique_ptr<MessageBody>(new_message(Code{1}, target)));
          self.deactivate();
        } else {
          requester = self.take_current_message();
        }
      }
    });
  });

  actor_system.spawn([&](ActorBehavior& self) {
    self.request(a, 1).on_reply({
      Code{1} - [&](Actor c) {
        self.request(c, 2, 45678).on_reply({
          Code{2} - [&](int v) {
            EXPECT_EQ(v, 45678 * 2);
            self.deactivate();
          }
        });
      }
    });
  });

  actor_system.spawn([&](ActorBehavior& self) {
    self.send(a, 0);
    self.receive_once({
      Code{2} - [&](int v) {
        self.reply(2, v * 2);
      }
    });
  });
}

GTEST_TEST(Request, UnorderedRequest) {
  ActorSystem actor_system;

  auto a = actor_system.spawn([&](ActorBehavior& self) {
    self.receive({
      zaf::Code{0} - [&](int x) {
        self.reply(0, x * 2);
      },
      zaf::Code{1} - [&]() {
        self.deactivate();
      }
    });
  });

  actor_system.spawn([&](ActorBehavior& self) {
    std::vector<ActorBehavior::RequestHandler> reqs;
    for (int i = 0; i < 10; i++) {
      reqs.emplace_back(self.request(a, 0, i));
    }
    self.send(a, 1);
    for (int i = 1; i < 10; i += 2) {
      reqs[i].on_reply({
        zaf::Code{0} - [&](int r) {
          EXPECT_EQ(r, i * 2);
        }
      });
    }
    for (int i = 0; i < 10; i += 2) {
      reqs[i].on_reply({
        zaf::Code{0} - [&](int r) {
          EXPECT_EQ(r, i * 2);
        }
      });
    }
  });
}
} // namespace zaf
