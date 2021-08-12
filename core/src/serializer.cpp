#include "zaf/serializer.hpp"

namespace zaf {
Serializer::Serializer(std::vector<char>& byte_buffer):
  bytes(byte_buffer) {
}

Serializer& Serializer::write() {
  return *this;
}

Deserializer::Deserializer(const char* off):
  offset(off) {
}

Deserializer::Deserializer(const std::vector<char>& bytes):
  offset(&bytes.front()) {
}
} // namespace zaf
