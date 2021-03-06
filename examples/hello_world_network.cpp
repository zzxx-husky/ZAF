#include "zaf/zaf.hpp"
#include "zaf/net_gate_client.hpp"

// define an actor by inherting zaf::ActorBehavior
class X : public zaf::ActorBehavior {
public:
  zaf::MessageHandlers behavior() override {
    return {
      zaf::Code{1} - [&](const std::string& hello) {
        LOG(INFO) << "Expected hello, received " << hello;
        this->reply(0, std::string("world"));
      },
      zaf::Code{2} - [&](const std::string& hw) {
        LOG(INFO) << "Expected hello world, received " << hw;
        this->deactivate();
      }
    };
  }
};

int main() {
  std::thread machine_a([]() {
    zaf::ActorSystem actor_system;
    zaf::NetGate gate{actor_system, "127.0.0.1", 12345};
    auto sender = actor_system.create_scoped_actor();
    zaf::NetGateClient client{gate.actor()};
    client.register_actor(*sender, "XActor", actor_system.spawn<X>());
  });
  std::thread machine_b([]() {
    zaf::ActorSystem actor_system;
    zaf::NetGate gate{actor_system, "127.0.0.1", 23456};
    actor_system.spawn([&, n = zaf::NetGateClient{gate.actor()}](zaf::ActorBehaviorX& self) {
      zaf::Actor x;
      n.lookup_actor(zaf::Requester{self}, "127.0.0.1:12345", "XActor").on_reply({
        n.on_lookup_actor_reply([&](std::string&, std::string&, zaf::Actor remote_x) {
          x = remote_x;
          self.deactivate();
          LOG(INFO) << "Fetched remote actor X!";
        })
      });
      self.send(x, 1, std::string("hello"));
      self.receive({
        zaf::Code{0} - [&](const std::string& world) {
          LOG(INFO) << "Expected world, received " << world;
          self.reply(2, std::string("hello world"));
          self.deactivate();
        }
      });
    });
  });
  machine_a.join();
  machine_b.join();
}

