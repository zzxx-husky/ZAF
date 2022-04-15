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

GTEST_TEST(ActorBehavior, ExtraRecvPoll) {
  ActorSystem actor_system;

  ActorBehavior actor1;
  actor1.initialize_actor(actor_system, actor_system);

  ActorBehavior actor2;
  actor2.initialize_actor(actor_system, actor_system);

  zmq::socket_t extra_recv;
  unsigned num_recvs = 0;
  {
    extra_recv = zmq::socket_t(
      actor_system.get_zmq_context(), zmq::socket_type::pull);
    extra_recv.bind("inproc://ActorBehavior:ExtraRecvPoll");
    actor2.add_recv_poll(extra_recv, [&]() {
      zmq::message_t message;
      EXPECT_TRUE(bool(extra_recv.recv(message)));
      EXPECT_EQ("Extra Hello World",
        std::string((char*) message.data(), message.size()));
      ++num_recvs;
      actor1.send(actor2, 0, std::string("Hello World"));
      actor2.remove_recv_poll(extra_recv, [&]() {
        extra_recv.unbind("inproc://ActorBehavior:ExtraRecvPoll");
        extra_recv.close();
      });
    });
  }

  {
    zmq::socket_t extra_send;
    extra_send = zmq::socket_t(
      actor_system.get_zmq_context(), zmq::socket_type::push);
    extra_send.connect("inproc://ActorBehavior:ExtraRecvPoll");
    extra_send.send(zmq::str_buffer("Extra Hello World"));
    extra_send.close();
  }

  actor2.receive({
    Code{0} - [&](const std::string& hw) {
      EXPECT_EQ(hw, "Hello World");
      ++num_recvs;
      actor2.deactivate();
    }
  });

  EXPECT_EQ(num_recvs, 2);
}

} // namespace zaf
