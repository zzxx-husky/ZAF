#include <chrono>

#include "zaf/zaf.hpp"

int main(int argc, char** argv) {
  int n_send = 3, n_recv = 3, n_msg = 1000000;

  zaf::ActorSystem system;
  std::vector<zaf::Actor> senders, receivers;
  for (int i = 0; i < n_send; i++) {
    senders.emplace_back(system.spawn([=](auto& self) {
      self.receive_once({
        zaf::Code{0} - [n_send, n_recv, n_msg, &self](const std::vector<zaf::Actor>& receivers) {
          for (int i = 0; i < n_msg; i++) {
            for (int j = 0; j < n_recv; j++) {
              self.send(receivers[j], 0, 0);
            }
          }
          for (int j = 0; j < n_recv; j++) {
            self.send(receivers[j], 1);
          }
        }
      });
      LOG(INFO) << "Sender " << i << " terminates.";
    }));
  }
  for (int i = 0; i < n_recv; i++) {
    receivers.emplace_back(system.spawn([=](auto& self) {
      int num_termination = 0;
      int num_msg_recv = 0;
      self.activate();
      self.receive({
        zaf::Code{0} - [&num_msg_recv](int) {
          ++num_msg_recv;
        },
        zaf::Code{1} - [i, n_send, &self, &num_termination, &num_msg_recv]() {
          if (++num_termination == n_send) {
            LOG(INFO) << "Receiver " << i << " receives " << num_msg_recv << " integers in total";
            self.deactivate();
          }
        }
      });
      LOG(INFO) << "Receiver " << i << " terminates.";
    }));
  }
  auto start = std::chrono::system_clock::now();
  {
    auto trigger = system.create_scoped_actor();
    for (int i = 0; i < n_send; i++) {
      trigger->send(senders[i], 0, receivers);
    }
  }
  system.await_all_actors_done();
  auto end = std::chrono::system_clock::now();
  LOG(INFO) << "Message to be recieved per receiver: " << (n_msg * n_send);
  LOG(INFO) << "Duration: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms";
}
