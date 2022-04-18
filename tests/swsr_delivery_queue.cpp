#include <condition_variable>
#include <mutex>

#include "zaf/swsr_delivery_queue.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(SWSRDeliveryQueue, ToSelfWithResize) {
  SWSRDeliveryQueue<int> queue;
  queue.resize(8);

  int s = 0;
  int r = 0;

  for (int i = 0; i < 100; i++) {
    for (int j = 0; j < 1000; j++) {
      queue.push(s++, SWSRDeliveryQueueFullStrategy::Resize);
    }
    int nr = queue.pop_some([&](int n) {
      EXPECT_EQ(r++, n);
    }, 10000u);
    EXPECT_EQ(nr, 1000);
  }

  EXPECT_EQ(s, r);
  EXPECT_EQ(r, 100 * 1000);
}

GTEST_TEST(SWSRDeliveryQueue, OneToOne) {
  SWSRDeliveryQueue<int> queue;
  queue.resize(16);

  int num_messages = 100000;

  std::mutex notifier_mutex;
  std::condition_variable notifier;
  bool reader_work = false;

  std::thread writer([&]() {
    for (int i = 0; i < num_messages; i++) {
      queue.push(i, [&]() {
        {
          std::lock_guard<std::mutex> _(notifier_mutex);
          reader_work = true;
        }
        notifier.notify_one();
      });
    }
  });

  std::thread reader([&]() {
    int num_receive = 0;
    while (num_receive < num_messages) {
      queue.pop_some([&](int n) {
        EXPECT_EQ(n, num_receive++);
      }, [&]() {
        std::unique_lock<std::mutex> _(notifier_mutex);
        notifier.wait(_, [&]() { return reader_work; });
      }, 10000);
    }
  });

  writer.join();
  reader.join();
}

GTEST_TEST(SWSRDeliveryQueue, OneToOneWithSomeDelay) {
  SWSRDeliveryQueue<int> queue;
  queue.resize(16);

  int num_messages = 100000;

  std::mutex notifier_mutex;
  std::condition_variable notifier;
  bool reader_work = false;

  std::thread writer([&]() {
    for (int i = 0; i < num_messages; i += 1000) {
      for (int j = 0; j < 1000; j++) {
        queue.push(i + j, [&]() {
          {
            std::lock_guard<std::mutex> _(notifier_mutex);
            reader_work = true;
          }
          notifier.notify_one();
        });
      }
      std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
  });

  std::thread reader([&]() {
    int num_receive = 0;
    while (num_receive < num_messages) {
      queue.pop_some([&](int n) {
        EXPECT_EQ(n, num_receive++);
      }, [&]() {
        std::unique_lock<std::mutex> _(notifier_mutex);
        notifier.wait(_, [&]() { return reader_work; });
      }, 10000);
    }
  });

  writer.join();
  reader.join();
}

GTEST_TEST(SWSRDeliveryQueue, MultiToMulti) {
  const int R = 3;
  const int W = 2;

  SWSRDeliveryQueue<int> queues[W][R];
  int num_messages = 100000;

  std::mutex notifier_mutex[W][R];
  std::condition_variable notifier[W][R];
  bool reader_work[W][R];

  for (int w = 0; w < W; w++) {
    for (int r = 0; r < R; r++) {
      queues[w][r].resize(16);
      reader_work[w][r] = false;
    }
  }

  std::thread writers[W];
  for (int w = 0; w < W; w++) {
    writers[w] = std::thread{[&, w]() {
      for (int i = 0; i < num_messages; i += 1000) {
        for (int j = 0; j < 1000; j++) {
          for (int r = 0; r < R; r++) {
            queues[w][r].push(i + j, [&]() {
              {
                std::lock_guard<std::mutex> _(notifier_mutex[w][r]);
                reader_work[w][r] = true;
              }
              notifier[w][r].notify_one();
            });
          }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
      }
    }};
  }

  std::thread readers[R];
  for (int r = 0; r < R; r++) {
    readers[r] = std::thread{[&, r]() {
      int num_receive[W] = {0};
      bool active[W] = {false};
      for (int total_receive = 0, exepcted_total_receive = num_messages * W;
           total_receive < exepcted_total_receive;) {
        for (int w = 0; w < W; w++) {
          if (active[w]) {
            continue;
          }
          std::unique_lock<std::mutex> _(notifier_mutex[w][r]);
          if (notifier[w][r].wait_for(_, std::chrono::milliseconds{1}, [&]() { return reader_work[w][r]; })) {
            active[w] = true;
          }
        }
        for (int w = 0; w < W; w++) {
          if (!active[w]) {
            continue;
          }
          queues[w][r].pop_some([&](int n) {
            EXPECT_EQ(n, num_receive[w]++);
            ++total_receive;
          }, [&]() {
            active[w] = false;
          }, 10000);
        }
      }
    }};
  }

  for (int w = 0; w < W; w++) {
    writers[w].join();
  }
  for (int r = 0; r < R; r++) {
    readers[r].join();
  }
}
} // namespace zaf
