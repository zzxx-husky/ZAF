#include "zaf/zaf.hpp"
#include "zaf/net_gate_client.hpp"

const zaf::Code AXReq{1};
const zaf::Code AXRep{2};
const zaf::Code AXInfoReq{3};
const zaf::Code AXInfoRep{4};
const zaf::Code BWInfoReq{5};
const zaf::Code BWInfoRep{6};
const zaf::Code BWReg{7};
const zaf::Code Termination{8};
const zaf::Code BWReq{9};
const zaf::Code BWRep{10};

class X : public zaf::ActorBehavior {
public:
  zaf::MessageHandlers behavior() override {
    return {
      AXReq - [&](int value) {
        LOG(INFO) << "a_x receives " << value;
        this->reply(AXRep, value * 2);
      },
      Termination - [&]() {
        this->deactivate();
      }
    };
  }
};

class Y : public zaf::ActorBehavior {
public:
  Y(zaf::NetGateClient&& n):
    n(std::move(n)) {
  }

  void start() override {
    n.lookup_actor(*this, "127.0.0.1:12345", "a_x");
  }

  zaf::MessageHandlers behavior() override {
    return {
      n.on_lookup_actor_reply([&](std::string&, std::string&, zaf::Actor remote_a_x) {
        a_x = remote_a_x;
        this->send(a_x, AXReq, 2468);
        if (a_x_requester) {
          this->send(a_x_requester, AXInfoRep, a_x.to_actor_info(a_x_requester));
          a_x_requester = nullptr;
        }
      }),
      AXRep - [&](int v) {
        LOG(INFO) << "b_y receives value: " << v << ", expected: " << 2468 * 2;
      },
      AXInfoReq - [&]() {
        if (a_x) {
          this->reply(AXInfoRep, a_x.to_actor_info(this->get_current_sender_actor()));
        } else {
          a_x_requester = this->get_current_sender_actor();
        }
      },
      BWReg - [&]() {
        b_w = this->get_current_sender_actor();
        if (b_w_requester) {
          this->send(b_w_requester, BWInfoRep, b_w.to_actor_info(b_w_requester));
          b_w_requester = nullptr;
        }
      },
      BWInfoReq - [&]() {
        if (b_w) {
          this->reply(BWInfoRep, b_w.to_actor_info(this->get_current_sender_actor()));
        } else {
          b_w_requester = this->get_current_sender_actor();
        }
      },
      Termination - [&]() {
        this->deactivate();
      }
    };
  }

  zaf::Actor a_x, a_x_requester, b_w, b_w_requester;
  zaf::NetGateClient n;
};

int main() {
  std::thread machine_a([]() {
    zaf::ActorSystem actor_system;
    zaf::NetGate gate{actor_system, "127.0.0.1", 12345};
    auto sender = actor_system.create_scoped_actor();
    zaf::NetGateClient client{gate.actor()};
    client.register_actor(*sender, "a_x", actor_system.spawn<X>());
  });
  std::thread machine_b([]() {
    zaf::ActorSystem actor_system;
    zaf::NetGate gate{actor_system, "127.0.0.1", 23456};
    auto y = actor_system.spawn<Y>(zaf::NetGateClient{gate.actor()});
    {
      auto sender = actor_system.create_scoped_actor();
      zaf::NetGateClient client{gate.actor()};
      client.register_actor(*sender, "b_y", y);
    }
    actor_system.spawn([&](zaf::ActorBehavior& w) {
      w.send(y, BWReg);
      w.receive_once({
        BWReq - [&](int v) {
          LOG(INFO) << "b_w receives " << v;
          w.reply(BWRep, ~v);
        }
      });
    });
  });
  std::thread machine_c([]() {
    zaf::ActorSystem actor_system;
    zaf::NetGate gate{actor_system, "127.0.0.1", 34567};
    actor_system.spawn([&, n = zaf::NetGateClient{gate.actor()}](zaf::ActorBehavior& self) {
      n.lookup_actor(self, "127.0.0.1:23456", "b_y");
      zaf::Actor b_y;
      self.receive({
        n.on_lookup_actor_reply([&](std::string&, std::string&, zaf::Actor remote_b_y) {
          b_y = remote_b_y;
          self.send(b_y, AXInfoReq);
        }),
        AXInfoRep - [&](const zaf::ActorInfo& a_x_info) {
          n.retrieve_actor(zaf::Requester{self}, a_x_info).on_reply(
            n.on_retrieve_actor_reply([&](const zaf::ActorInfo&, const zaf::Actor& a) {
              self.send(a, AXReq, 1357);
            })
          );
        },
        AXRep - [&](int v) {
          LOG(INFO) << "c_z eceives value: " << v << ", expected: " << 1357 * 2;
          self.reply(Termination);
          self.send(b_y, BWInfoReq);
        },
        BWInfoRep - [&](const zaf::ActorInfo& b_w_info) {
          n.retrieve_actor(zaf::Requester{self}, b_w_info).on_reply(
            n.on_retrieve_actor_reply([&](const zaf::ActorInfo&, const zaf::Actor& a) {
              self.send(a, BWReq, 1357);
            })
          );
        },
        BWRep - [&](int v) {
          LOG(INFO) << "c_z receives value: " << v << ", expected: " << (~1357);
          self.send(b_y, Termination);
          self.deactivate();
        }
      });
    });
  });
  machine_a.join();
  machine_b.join();
  machine_c.join();
}
