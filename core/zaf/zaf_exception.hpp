#pragma once

#include <exception>
#include <iostream>
#include <stdexcept>

#include "to_string.hpp"

namespace zaf {
class ZAFException : public std::exception {
public:
  ZAFException() = default;

  ZAFException(const std::string& msg);
  ZAFException(std::string&& msg);

  template<typename ... ArgT>
  ZAFException(ArgT&& ... args):
    ZAFException(to_string(std::forward<ArgT>(args) ...)) {
  }

  const char* what() const noexcept override;

private:
  const std::string msg;
};

void print_exception(const std::exception& e, int level = 0);
void print_exception(std::ostream& o, const std::exception& e, int level = 0);
} // namespace zaf
