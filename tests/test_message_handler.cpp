#include <string>
#include <vector>

#include "zaf/message_handler.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(MessageHandler, HandlersWithoutArg) {
  int x = 0;
  MessageHandlers handlers = {
    zaf::Code{0} - [&]() {
      x++;
    },
    zaf::Code{1} - [&]() {
      x *= 2;
    }
  };

  {
    auto m = make_message(0);
    handlers.process(*m);
    EXPECT_EQ(x, 1);
    delete m;
  }
  {
    auto m = make_message(1);
    handlers.process(*m);
    EXPECT_EQ(x, 2);
    handlers.process(*m);
    EXPECT_EQ(x, 4);
    delete m;
  }
  {
    auto m = make_message(2);
    EXPECT_ANY_THROW(handlers.process(*m));
    delete m;
  }
}

GTEST_TEST(MessageHandler, HandlersWithArgs) {
  int x = 0;
  MessageHandlers handlers = {
    zaf::Code{0} - [&](int a) {
      x += a;
    },
    zaf::Code{1} - [&](int b) {
      x *= b;
    }
  };

  {
    auto m = make_message(0, 10);
    handlers.process(*m);
    EXPECT_EQ(x, 10);
    delete m;
  }
  {
    auto m = make_message(1, 3);
    handlers.process(*m);
    EXPECT_EQ(x, 30);
    handlers.process(*m);
    EXPECT_EQ(x, 90);
    delete m;
  }
  {
    auto m = make_message(2);
    EXPECT_ANY_THROW(handlers.process(*m));
    delete m;
  }
}

GTEST_TEST(MessageHandler, HandlersWithObjArgs) {
  MessageHandlers handlers = {
    zaf::Code{0} - [&](const std::string& a) {
      EXPECT_EQ(a.size(), 11);
      EXPECT_EQ(a, "Hello World");
    },
    zaf::Code{1} - [&](const std::vector<int>& b) {
      EXPECT_EQ(b.size(), 5);
      EXPECT_EQ(b, (std::vector<int>{1,2,3,4,5}));
    },
    zaf::Code{2} - [&](int* x) {
      *x *= 2;
    }
  };
  {
    auto m = make_message(0, std::string("Hello World"));
    handlers.process(*m);
    delete m;
  }
  {
    auto m = make_message(1, std::vector<int>{1,2,3,4,5});
    handlers.process(*m);
    delete m;
  }
  {
    int b = 10;
    auto m = make_message(2, &b);
    handlers.process(*m);
    EXPECT_EQ(b, 20);
    delete m;
  }
}

} // namespace zaf
