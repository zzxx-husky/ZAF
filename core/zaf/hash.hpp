#pragma once

#include <functional>
#include <utility>

namespace zaf {
inline size_t hash_combine() {
  return 0;
}

template<typename T>
inline size_t hash_combine(T&& x) {
  return std::hash<std::decay_t<T>>{}(std::forward<T>(x));
}

template<typename T, typename ... ArgT>
size_t hash_combine(T&& x, ArgT&& ... args) {
  auto h = hash_combine(x);
  return hash_combine(std::forward<ArgT>(args) ...) + 0x9e3779b9 + (h << 6) + (h >> 2);
}
} // namespace zaf
