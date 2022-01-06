#include <array>
#include <deque>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "zaf/actor.hpp"
#include "zaf/actor_behavior.hpp"
#include "zaf/count_pointer.hpp"
#include "zaf/macros.hpp"
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
  EXPECT_FALSE((traits::all_serializable<std::pair<Z, int>>::value));
  EXPECT_TRUE((traits::all_serializable<int, int, int, int, int>::value));
  EXPECT_TRUE((traits::all_serializable<const int, int, int, int, const char>::value));
  EXPECT_FALSE((traits::all_serializable<Z, int, int, int, const char>::value));
  // Iterable
  EXPECT_TRUE(traits::all_serializable<std::vector<int>>::value);
  EXPECT_TRUE(traits::all_serializable<std::vector<bool>>::value);
  EXPECT_TRUE(traits::all_serializable<std::deque<int>>::value);
  EXPECT_TRUE(traits::all_serializable<std::list<int>>::value);
  EXPECT_TRUE((traits::all_serializable<std::map<int, int>>::value));
  EXPECT_TRUE(traits::all_serializable<std::set<int>>::value);
  EXPECT_TRUE((traits::all_serializable<std::unordered_map<int, int>>::value));
  EXPECT_TRUE(traits::all_serializable<std::unordered_set<int>>::value);
  EXPECT_TRUE((traits::all_serializable<DefaultHashMap<int, int>>::value));
  EXPECT_TRUE(traits::all_serializable<DefaultHashSet<int>>::value);
  EXPECT_TRUE((traits::all_serializable<std::array<int, 4>>::value));
  EXPECT_FALSE(traits::all_serializable<std::vector<Z>>::value);
  EXPECT_FALSE(traits::all_serializable<std::deque<Z>>::value);
  EXPECT_FALSE(traits::all_serializable<std::list<Z>>::value);
  EXPECT_FALSE((traits::all_serializable<std::map<int, Z>>::value));
  EXPECT_FALSE(traits::all_serializable<std::set<Z>>::value);
  EXPECT_FALSE((traits::all_serializable<std::unordered_map<int, Z>>::value));
  EXPECT_FALSE(traits::all_serializable<std::unordered_set<Z>>::value);
  EXPECT_FALSE((traits::all_serializable<DefaultHashMap<int, Z>>::value));
  EXPECT_FALSE(traits::all_serializable<DefaultHashSet<Z>>::value);
  EXPECT_TRUE(traits::all_serializable<std::unique_ptr<int>>::value);
  EXPECT_TRUE(traits::all_serializable<std::shared_ptr<int>>::value);
  EXPECT_TRUE(traits::all_serializable<CountPointer<int>>::value);
  EXPECT_FALSE(traits::all_serializable<std::unique_ptr<Z>>::value);
  EXPECT_FALSE(traits::all_serializable<std::shared_ptr<Z>>::value);
  EXPECT_FALSE(traits::all_serializable<CountPointer<Z>>::value);
  EXPECT_TRUE((traits::all_serializable<std::tuple<>, std::tuple<X>, std::tuple<int>>::value));
  EXPECT_FALSE((traits::all_serializable<std::tuple<Z>, std::tuple<Z*>>::value));
  // Actor
  EXPECT_TRUE((traits::all_serializable<ActorInfo>::value));
  EXPECT_FALSE((traits::all_serializable<Actor>::value));
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
    std::vector<bool> a = {0, 1, 0, 1, 0, 1};
    s.write(a);
    Deserializer d(bytes);
    auto b = d.read<std::vector<bool>>();
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
    Code{0} - [&]() {
      x++;
    },
    Code{1} - [&]() {
      x *= 2;
    }
  };

  {
    auto m = make_message(nullptr, 0);
    auto s = make_serialized_message(m);
    handlers.process(s);
    EXPECT_EQ(x, 1);
  }
  {
    auto m = make_message(nullptr, 1);
    auto s = make_serialized_message(m);
    handlers.process(s);
    EXPECT_EQ(x, 2);
    handlers.process(s);
    EXPECT_EQ(x, 4);
  }
  {
    auto m = make_message(nullptr, 2);
    auto s = make_serialized_message(m);
    EXPECT_ANY_THROW(handlers.process(s));
  }
}

GTEST_TEST(SerializedMessage, HandlersWithArgs) {
  int x = 0;
  MessageHandlers handlers = {
    Code{0} - [&](int a) {
      x += a;
    },
    Code{1} - [&](int b) {
      x *= b;
    }
  };

  {
    auto m = make_message(nullptr, 0, 10);
    auto s = make_serialized_message(m);
    handlers.process(s);
    EXPECT_EQ(x, 10);
  }
  {
    auto m = make_message(nullptr, 1, 3);
    auto s = make_serialized_message(m);
    handlers.process(s);
    EXPECT_EQ(x, 30);
    handlers.process(s);
    EXPECT_EQ(x, 90);
  }
  {
    auto m = make_message(nullptr, 2);
    auto s = make_serialized_message(m);
    EXPECT_ANY_THROW(handlers.process(s));
  }
}

GTEST_TEST(SerializedMessage, HandlersWithObjArgs) {
  MessageHandlers handlers = {
    Code{0} - [&](const std::string& a) {
      EXPECT_EQ(a.size(), 11);
      EXPECT_EQ(a, "Hello World");
    },
    Code{1} - [&](const std::vector<int>& b) {
      EXPECT_EQ(b.size(), 5);
      EXPECT_EQ(b, (std::vector<int>{1,2,3,4,5}));
    },
    Code{2} - [&](int* x) {
      *x *= 2;
    }
  };
  {
    auto m = make_message(nullptr, 0, std::string("Hello World"));
    auto s = make_serialized_message(m);
    handlers.process(s);
  }
  {
    auto m = make_message(nullptr, 1, std::vector<int>{1,2,3,4,5});
    auto s = make_serialized_message(m);
    handlers.process(s);
  }
}

GTEST_TEST(SerializedMessage, MessageInMessage) {
  MessageHandlers handlers = {
    Code{0} - [&](const std::string& a) {
      EXPECT_EQ(a.size(), 11);
      EXPECT_EQ(a, "Hello World");
    },
    Code{1} - [&](const std::vector<int>& b) {
      EXPECT_EQ(b.size(), 5);
      EXPECT_EQ(b, (std::vector<int>{1,2,3,4,5}));
    },
    Code{2} - [&](int* x) {
      *x *= 2;
    }
  };
  MessageHandlers outer_handlers = {
    Code{100} - [&](std::unique_ptr<MessageBody>& m) {
      handlers.process_body(*m);
    }
  };
  {
    auto m = make_message(nullptr, 100,
      std::unique_ptr<MessageBody>(new_message(0, std::string("Hello World"))));
    auto s = make_serialized_message(m);
    outer_handlers.process(s);
  }
  {
    auto m = make_message(nullptr, 100,
      std::unique_ptr<MessageBody>(new_message(1, std::vector<int>{1,2,3,4,5})));
    auto s = make_serialized_message(m);
    outer_handlers.process(s);
  }
}

GTEST_TEST(SerializedMessage, Tuple) {
  {
    std::vector<char> bytes;
    Serializer s(bytes);
    auto a = std::make_tuple(1, 2, 3);
    s.write(a);
    Deserializer d(bytes);
    auto b = d.read<std::tuple<int, int, int>>();
    EXPECT_EQ(a, b);
  }
  {
    std::vector<char> bytes;
    Serializer s(bytes);
    auto a = std::make_tuple();
    s.write(a);
    Deserializer d(bytes);
    auto b = d.read<std::tuple<>>();
    EXPECT_EQ(a, b);
  }
}
} // namespace zaf
