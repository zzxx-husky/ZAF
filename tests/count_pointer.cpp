#include "zaf/count_pointer.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(CountPointer, NullPointer) {
  auto pointer = nullptr;
}

GTEST_TEST(CountPointer, Assignment) {
  auto a = CountPointer<int>(-1);
  auto b = a;
  EXPECT_EQ(-1, *b);

  CountPointer<int> c;
  {
    auto x = CountPointer<int>(2);
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
  auto a = CountPointer<internal::CountPointerClass>(internal::CountPointerClass());
  EXPECT_EQ(1, internal::CountPointerClass::count);
  a = nullptr;
  EXPECT_EQ(0, internal::CountPointerClass::count);

  a = CountPointer<internal::CountPointerClass>(internal::CountPointerClass());
  auto b = a;
  EXPECT_EQ(1, internal::CountPointerClass::count);
  auto c = b;
  EXPECT_EQ(1, internal::CountPointerClass::count);
  b = CountPointer<internal::CountPointerClass>(internal::CountPointerClass());
  EXPECT_EQ(2, internal::CountPointerClass::count);
  a = CountPointer<internal::CountPointerClass>(internal::CountPointerClass());
  EXPECT_EQ(3, internal::CountPointerClass::count);
  c = nullptr;
  a = nullptr;
  b = nullptr;
  EXPECT_EQ(0, internal::CountPointerClass::count);
}
} // namespace zaf
