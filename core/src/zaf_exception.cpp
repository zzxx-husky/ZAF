#include <iostream>

#include "zaf/zaf_exception.hpp"

namespace zaf {
ZAFException::ZAFException(const std::string& msg):
  msg(msg) {
}

ZAFException::ZAFException(std::string&& msg):
  msg(std::move(msg)) {
}

const char* ZAFException::what() const noexcept {
  return msg.data();
}

void print_exception(const std::exception& e, int level) {
  print_exception(std::cerr, e, level);
}

void print_exception(std::ostream& o, const std::exception& e, int level) {
  o << std::string(level, ' ') << "exception: " << e.what() << std::endl;
  try {
    std::rethrow_if_nested(e);
  } catch (const std::exception& e) {
    print_exception(o, e, level + 1);
  } catch (...) {
    o << std::string(level + 1, ' ') << "exception: unknown" << std::endl;
  }
}
} // namespace zaf
