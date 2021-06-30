#include <typeinfo>

#include "zaf/callable_signature.hpp"

#include "gtest/gtest.h"

namespace zaf {
GTEST_TEST(CallableSignature, Lambda) {
  auto x = [](int x) { return x + 1; };
  EXPECT_TRUE(traits::is_callable<decltype(x)>::value);
  EXPECT_EQ(
    typeid(int),
    typeid(traits::is_callable<decltype(x)>::ret_t)
  );
  EXPECT_EQ(
    typeid(int),
    typeid(traits::is_callable<decltype(x)>::args_t::arg_t<0>)
  );
  EXPECT_NE(
    typeid(unsigned int),
    typeid(traits::is_callable<decltype(x)>::args_t::arg_t<0>)
  );
}

GTEST_TEST(CallableSignature, LambdaWithoutArg) {
  int z = 0;
  auto x = [&]() { z + 1; };
  EXPECT_TRUE(traits::is_callable<decltype(x)>::value);
  EXPECT_EQ(traits::is_callable<decltype(x)>::args_t::size, 0);
  EXPECT_EQ(
    typeid(void),
    typeid(traits::is_callable<decltype(x)>::ret_t)
  );
}

GTEST_TEST(CallableSignature, LambdaWithMultiArgs) {
  int a_global_value;
  auto x = [&](int x, double y, float z) { a_global_value = x + y + z; };
  EXPECT_TRUE(traits::is_callable<decltype(x)>::value);
  EXPECT_EQ(
    typeid(void),
    typeid(traits::is_callable<decltype(x)>::ret_t)
  );
  EXPECT_EQ(
    typeid(int),
    typeid(traits::is_callable<decltype(x)>::args_t::arg_t<0>)
  );
  EXPECT_EQ(
    typeid(double),
    typeid(traits::is_callable<decltype(x)>::args_t::arg_t<1>)
  );
  EXPECT_EQ(
    typeid(float),
    typeid(traits::is_callable<decltype(x)>::args_t::arg_t<2>)
  );
}

GTEST_TEST(CallableSignature, AutoLambda) {
  auto x = [](auto x) { return x + 1; };
  EXPECT_FALSE(traits::is_callable<decltype(x)>::value);
}

GTEST_TEST(CallableSignature, Function) {
  void func(int x);
  EXPECT_TRUE(traits::is_callable<decltype(func)>::value);
  EXPECT_EQ(
    typeid(int),
    typeid(traits::is_callable<decltype(func)>::args_t::arg_t<0>)
  );
  EXPECT_EQ(
    typeid(void),
    typeid(traits::is_callable<decltype(func)>::ret_t)
  );
}

GTEST_TEST(CallableSignature, MemberFunction) {
  struct X {
    void func(char x);
  };
  EXPECT_TRUE(traits::is_callable<decltype(&X::func)>::value);
  EXPECT_EQ(
    typeid(char),
    typeid(traits::is_callable<decltype(&X::func)>::args_t::arg_t<0>)
  );
  EXPECT_EQ(
    typeid(void),
    typeid(traits::is_callable<decltype(&X::func)>::ret_t)
  );
}

GTEST_TEST(CallableSignature, Functor) {
  struct X {
    void operator()(double x);
  };
  EXPECT_TRUE(traits::is_callable<X>::value);
  EXPECT_EQ(
    typeid(double),
    typeid(traits::is_callable<X>::args_t::arg_t<0>)
  );
  EXPECT_EQ(
    typeid(void),
    typeid(traits::is_callable<X>::ret_t)
  );
}

GTEST_TEST(CallableSignature, FunctorWithMultiCalls) {
  struct X {
    void operator()(double x);
    void operator()(int x);
  };
  EXPECT_FALSE(traits::is_callable<X>::value);
}

} // namespace zaf
