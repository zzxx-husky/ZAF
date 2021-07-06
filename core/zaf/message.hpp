#pragma once

#include <memory>
#include <tuple>
#include <vector>

namespace zaf {
class Message {
public:
  Message(size_t code);

  size_t get_code() const;

  virtual size_t size() const = 0;

  virtual ~Message();

  virtual void fill_with_element_addrs(std::vector<std::uintptr_t>& addrs) const = 0;

private:
  size_t code;
};

template<typename MessageContent>
struct TypedMessage : public Message {
  TypedMessage(size_t code, MessageContent&& content):
    Message(code),
    content(std::move(content)) {
  }

  size_t size() const override {
    return std::tuple_size<MessageContent>::value;
  }

private:
  template<size_t i, size_t n>
  void fill_addrs(std::vector<std::uintptr_t>& addrs) const {
    if constexpr (i < n) {
      addrs[i] = reinterpret_cast<std::uintptr_t>(&std::get<i>(content));
      fill_addrs<i + 1, n>(addrs);
    }
  }

  void fill_with_element_addrs(std::vector<std::uintptr_t>& addrs) const override {
    fill_addrs<0, std::tuple_size<MessageContent>::value>(addrs);
  }

  // a std::tuple that contains multiple members
  MessageContent content;
};

template<typename ... ArgT>
auto make_message(size_t code, ArgT&& ... args) {
  auto content = std::make_tuple(std::forward<ArgT>(args)...);
  return new TypedMessage<decltype(content)>(code, std::move(content));
}
} // namespace zaf
