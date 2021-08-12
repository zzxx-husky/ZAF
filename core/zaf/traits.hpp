#pragma once

#include <type_traits>

namespace zaf {
namespace traits {
template<typename T>
inline T& remove_const(const T& v) {
  return const_cast<T&>(v);
}

template<typename T>
inline T* remove_const(const T* v) {
  return const_cast<T*>(v);
}

template<typename T>
struct is_pair : public std::false_type {};

template<typename A, typename B>
struct is_pair<std::pair<A, B>> : public std::true_type {
  using first_type = A;
  using second_type = B;
};

namespace impl {
template<typename T>
auto is_iterable(int) -> decltype(
  ++std::declval<decltype(std::begin(std::declval<T&>()))&>(),
  *std::declval<decltype(std::begin(std::declval<T&>()))&>(),
  std::true_type{}
);

template<typename>
std::false_type is_iterable(...);

template<typename T, typename I>
auto has_emplace_back(int) -> decltype(
  std::declval<T&>().emplace_back(std::declval<I&>()),
  std::true_type{}
);

template<typename, typename>
std::false_type has_emplace_back(...);

template<typename T, typename I>
auto has_emplace(int) -> decltype(
  std::declval<T&>().emplace(std::declval<I&>()),
  std::true_type{}
);

template<typename, typename>
std::false_type has_emplace(...);

template<typename T, typename I>
auto has_insert(int) -> decltype(
  std::declval<T&>().insert(std::declval<I&>()),
  std::true_type{}
);

template<typename, typename>
std::false_type has_insert(...);


template<typename T>
auto has_reserve(int) -> decltype(
  std::declval<T&>().reserve(1),
  std::true_type{}
);

template<typename>
std::false_type has_reserve(...);
} // namespace impl

template<typename I>
using is_iterable = decltype(impl::is_iterable<I>(0));

template<typename T, typename I>
using has_emplace_back = decltype(impl::has_emplace_back<T, I>(0));

template<typename T, typename I>
using has_emplace = decltype(impl::has_emplace<T, I>(0));

template<typename T, typename I>
using has_insert = decltype(impl::has_insert<T, I>(0));

template<typename T>
using has_reserve = decltype(impl::has_reserve<T>(0));

template<typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;
} // namespace traits

// https://en.cppreference.com/w/cpp/utility/variant/visit
template<class... Ts> struct overloaded : Ts... { using Ts::operator()...; };
template<class... Ts> overloaded(Ts...) -> overloaded<Ts...>;
} // namespace zaf
