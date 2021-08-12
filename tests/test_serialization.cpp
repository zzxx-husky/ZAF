#include <array>
#include <deque>
#include <list>
#include <map>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "zaf/actor_behavior.hpp"
#include "zaf/message.hpp"
#include "zaf/message_handler.hpp"
#include "zaf/serializer.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(SerializedMessage, Traits) {
  struct X {};
  struct Y { int a; };
  struct Z { private: std::string b; };
  // POD or basic types
  EXPECT_TRUE(traits::all_serializable<>::value);
  EXPECT_TRUE(traits::all_serializable<int>::value);
  EXPECT_TRUE(traits::all_serializable<const int&>::value);
  EXPECT_TRUE(traits::all_serializable<double>::value);
  EXPECT_TRUE(traits::all_serializable<const double&>::value);
  EXPECT_TRUE(traits::all_serializable<X>::value);
  EXPECT_TRUE(traits::all_serializable<Y>::value);
  EXPECT_FALSE(traits::all_serializable<Z>::value);
  EXPECT_TRUE((traits::all_serializable<std::pair<int, int>>::value));
  EXPECT_TRUE((traits::all_serializable<std::pair<const int, int>>::value));
  EXPECT_TRUE((traits::all_serializable<int, int, int, int, int>::value));
  EXPECT_TRUE((traits::all_serializable<const int, int, int, int, const char>::value));
  EXPECT_FALSE((traits::all_serializable<Z, int, int, int, const char>::value));
  // Iterable
  EXPECT_TRUE(traits::all_serializable<std::vector<int>>::value);
  EXPECT_TRUE(traits::all_serializable<std::deque<int>>::value);
  EXPECT_TRUE(traits::all_serializable<std::list<int>>::value);
  EXPECT_TRUE((traits::all_serializable<std::map<int, int>>::value));
  EXPECT_TRUE(traits::all_serializable<std::set<int>>::value);
  EXPECT_TRUE((traits::all_serializable<std::unordered_map<int, int>>::value));
  EXPECT_TRUE(traits::all_serializable<std::unordered_set<int>>::value);
  EXPECT_FALSE((traits::all_serializable<std::array<int, 4>>::value));
  // a pointer pointing to a local variable
  EXPECT_EQ(traits::all_serializable<int*>::value, traits::all_serializable<int>::value);
  EXPECT_EQ(traits::all_serializable<std::vector<int>*>::value, traits::all_serializable<std::vector<int>>::value);
  EXPECT_EQ(traits::all_serializable<MessageHandlers*>::value, traits::all_serializable<MessageHandlers>::value);
  EXPECT_EQ(traits::all_serializable<ActorBehavior*>::value, traits::all_serializable<ActorBehavior>::value);
}

GTEST_TEST(SerializedMessage, Basic) {
  {
    std::vector<char> bytes;
    Serializer s(bytes);
    auto a = std::vector<int>{1, 2, 3, 4, 5, 6};
    s.write(a);
    Deserializer d(bytes);
    auto b = d.read<std::vector<int>>();
    EXPECT_EQ(a, b);
  }
  {
    std::vector<char> bytes;
    Serializer s(bytes);
    std::pair<const int, int> a{1, 2};
    s.write(a);
    Deserializer d(bytes);
    auto b = d.read<std::pair<const int, int>>();
    EXPECT_EQ(a, b);
  }
  {
    std::vector<char> bytes;
    Serializer s(bytes);
    auto a = std::map<int, std::string> {
      {0, "hello"},
      {1, "world"}
    };
    s.write(a);
    Deserializer d(bytes);
    auto b = d.read<std::map<int, std::string>>();
    EXPECT_EQ(a, b);
  }
  {
    std::vector<char> bytes;
    Serializer s(bytes);
    auto a = std::string{"hello world"};
    s.write(a);
    Deserializer d(bytes);
    auto b = d.read<std::string>();
    EXPECT_EQ(a, b);
  }
  {
    std::vector<char> bytes;
    Serializer s(bytes);
    int* a = new int(99);
    s.write(a);
    Deserializer d(bytes);
    auto b = d.read<int*>();
    EXPECT_EQ(*a, *b);
    delete a;
    delete b;
  }
}

GTEST_TEST(SerializedMessage, EmptyMessageContent) {
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
    auto s = m->serialize();
    handlers.process(*s);
    EXPECT_EQ(x, 1);
    delete s;
    delete m;
  }
  {
    auto m = make_message(1);
    auto s = m->serialize();
    handlers.process(*s);
    EXPECT_EQ(x, 2);
    handlers.process(*s);
    EXPECT_EQ(x, 4);
    delete s;
    delete m;
  }
  {
    auto m = make_message(2);
    auto s = m->serialize();
    EXPECT_ANY_THROW(handlers.process(*s));
    delete s;
    delete m;
  }
}

GTEST_TEST(SerializedMessage, HandlersWithArgs) {
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
    auto s = m->serialize();
    handlers.process(*s);
    EXPECT_EQ(x, 10);
    delete s;
    delete m;
  }
  {
    auto m = make_message(1, 3);
    auto s = m->serialize();
    handlers.process(*s);
    EXPECT_EQ(x, 30);
    handlers.process(*s);
    EXPECT_EQ(x, 90);
    delete s;
    delete m;
  }
  {
    auto m = make_message(2);
    auto s = m->serialize();
    EXPECT_ANY_THROW(handlers.process(*s));
    delete s;
    delete m;
  }
}

GTEST_TEST(SerializedMessage, HandlersWithObjArgs) {
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
    auto s = m->serialize();
    handlers.process(*s);
    delete s;
    delete m;
  }
  {
    auto m = make_message(1, std::vector<int>{1,2,3,4,5});
    auto s = m->serialize();
    handlers.process(*s);
    delete s;
    delete m;
  }
}
} // namespace zaf
