#pragma once

#include <sstream>

namespace zaf {
namespace impl {
template<typename Arg, typename ... ArgT>
inline void to_string(std::ostringstream& oss, Arg&& arg, ArgT&& ... args);

template<typename Arg>
inline void to_string(std::ostringstream& oss, Arg&& arg);

template<typename Arg, typename ... ArgT>
inline void to_string(std::ostringstream& oss, Arg&& arg, ArgT&& ... args) {
  oss << arg;
  to_string(oss, std::forward<ArgT>(args) ...);
}

template<typename Arg>
inline void to_string(std::ostringstream& oss, Arg&& arg) {
  oss << arg;
}
} // namespace impl

template<typename ... ArgT>
std::string to_string(ArgT&& ... args) {
  std::ostringstream oss;
  impl::to_string(oss, std::forward<ArgT>(args) ...);
  return oss.str();
}
} // namespace zaf

