#include <algorithm>
#include <vector>

#include "zaf/serializer.hpp"
#include "zaf/zaf_exception.hpp"

namespace zaf {
Serializer::Serializer(std::vector<char>& byte_buffer):
  bytes(byte_buffer),
  wptr(bytes.size()) {
}

void Serializer::write_bytes(const char* b, size_t n) {
  if (wptr == bytes.size()) {
    bytes.insert(bytes.end(), b, b + n);
  } else {
    auto copy_len = std::min(bytes.size() - wptr, n);
    std::copy(b, b + copy_len, &bytes.front() + wptr);
    if (copy_len < n) {
      bytes.insert(bytes.end(), b + copy_len, b + n);
    }
  }
  wptr += n;
}

Serializer& Serializer::write() {
  return *this;
}

void Serializer::move_write_ptr_to(size_t ptr) {
  if (ptr > bytes.size()) {
    throw ZAFException("Invalid write pointer: ", ptr,
      " (length of byte buffer: ", bytes.size(), ").");
  }
  wptr = ptr;
}

void Serializer::move_write_ptr_to_end() {
  wptr = bytes.size();
}

const std::vector<char>& Serializer::get_underlying_bytes() const {
  return bytes;
}

size_t Serializer::size() const {
  return bytes.size();
}

Deserializer::Deserializer(const char* off):
  offset(off) {
}

Deserializer::Deserializer(const std::vector<char>& bytes):
  offset(&bytes.front()) {
}
} // namespace zaf
