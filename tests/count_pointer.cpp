#include "zaf/count_pointer.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(CountPointer, NullPointer) {
  CountPointer<int> pointer = nullptr;
}

GTEST_TEST(CountPointer, CopyAndMove) {
  auto a = make_count<int>(-1);
  {
    a = a;
    EXPECT_EQ(*a, -1);
  }
  {
    a = std::move(a);
    EXPECT_EQ(*a, -1);
  }
  {
    auto b = std::move(a);
    EXPECT_FALSE(bool(a));
    EXPECT_TRUE(bool(b));
    EXPECT_EQ(*b, -1);
  }
}

GTEST_TEST(CountPointer, Assignment) {
  auto a = make_count<int>(-1);
  auto b = a;
  EXPECT_EQ(-1, *b);

  CountPointer<int> c;
  {
    auto x = make_count<int>(2);
    c = x;
  }
  EXPECT_EQ(*CountPointer<int>(c), 2);
}

namespace internal {
class CountPointerClass {
public:
  static unsigned count;

  CountPointerClass() {
    ++count;
  }

  CountPointerClass(const CountPointerClass &) {
    ++count;
  }

  CountPointerClass(CountPointerClass &&) {
    ++count;
  }

  ~CountPointerClass() {
    --count;
  }
};

unsigned CountPointerClass::count = 0;
}

GTEST_TEST(CountPointer, CustomizedClass) {
  auto a = make_count<internal::CountPointerClass>(internal::CountPointerClass());
  EXPECT_EQ(1, internal::CountPointerClass::count);
  a = nullptr;
  EXPECT_EQ(0, internal::CountPointerClass::count);

  a = make_count<internal::CountPointerClass>(internal::CountPointerClass());
  auto b = a;
  EXPECT_EQ(1, internal::CountPointerClass::count);
  auto c = b;
  EXPECT_EQ(1, internal::CountPointerClass::count);
  b = make_count<internal::CountPointerClass>(internal::CountPointerClass());
  EXPECT_EQ(2, internal::CountPointerClass::count);
  a = make_count<internal::CountPointerClass>(internal::CountPointerClass());
  EXPECT_EQ(3, internal::CountPointerClass::count);
  c = nullptr;
  a = nullptr;
  b = nullptr;
  EXPECT_EQ(0, internal::CountPointerClass::count);
}
} // namespace zaf
