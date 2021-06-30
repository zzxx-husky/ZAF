#include "zaf/message.hpp"

namespace zaf {
Message::Message(size_t code): code(code) {}

size_t Message::get_code() const { return code; }

Message::~Message() {}
} // namespace zaf

