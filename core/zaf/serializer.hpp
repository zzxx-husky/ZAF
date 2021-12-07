#pragma once

#include <cstring>
#include <string>
#include <tuple>
#include <type_traits>
#include <vector>

#include "callable_signature.hpp"
#include "traits.hpp"

namespace zaf {
class Serializer {
public:
  Serializer(std::vector<char>& byte_buffer);

  void write_bytes(const char* b, size_t n);

  template<typename I>
  inline void write_pod(I&& x) {
    write_bytes(reinterpret_cast<const char*>(&x), sizeof(x));
  }

  Serializer& write();

  template<typename T, typename ... Ts>
  Serializer& write(T&& t, Ts&& ... ts);

  void move_write_ptr_to(size_t ptr);
  void move_write_ptr_to_end();

  const std::vector<char>& get_underlying_bytes() const;

  size_t size() const;

private:
  std::vector<char>& bytes;
  size_t wptr;
};

class Deserializer {
public:
  Deserializer(const char* offset);
  Deserializer(const std::vector<char>&);

  inline void read_bytes(void* b, size_t n) {
    std::memcpy(b, offset, n);
    offset += n;
  }

  template<typename I>
  inline void read_pod(I& x) {
    read_bytes(&x, sizeof(x));
  }

  template<typename T>
  T read();

  Deserializer& read();

  template<typename T>
  Deserializer& read(T& t);

  template<typename T, typename ... Ts>
  Deserializer& read(T& t, Ts&& ... ts);

private:
  const char* offset;
};

namespace traits {
template<typename ... ArgT>
struct all_serializable;
} // namespace traits
} // namespace zaf

#include "serialization.hpp"

namespace zaf {
template<typename T, typename ... Ts>
Serializer& Serializer::write(T&& t, Ts&& ... ts) {
  serialize(*this, std::forward<T>(t));
  return write(std::forward<Ts>(ts)...);
}

template<typename T>
T Deserializer::read() {
  return deserialize<T>(*this);
}

template<typename T>
Deserializer& Deserializer::read(T& t) {
  deserialize(*this, t);
  return *this;
}

template<typename T, typename ... Ts>
Deserializer& Deserializer::read(T& t, Ts&& ... ts) {
  deserialize(*this, t);
  return read(std::forward<Ts>(ts) ...);
}

namespace traits {
template<typename ... ArgT>
struct NonSerializableAnalyzer;

template<>
struct NonSerializableAnalyzer<> {
  static void to_string(std::string&) {}
  static std::string to_string() { return ""; }
};

template<typename Arg>
struct NonSerializableAnalyzer<Arg> {
  static void to_string(std::string& str) {
    if constexpr (!traits::is_savable<Arg>::value || !traits::is_loadable<Arg>::value) {
      if (!str.empty()) {
        str.append(", ");
      }
      str.append(typeid(Arg).name());
    }
  }

  static std::string to_string() {
    std::string s;
    to_string(s);
    return s;
  }
};

template<typename Arg, typename ... ArgT>
struct NonSerializableAnalyzer<Arg, ArgT ...> {
  static void to_string(std::string& str) {
    NonSerializableAnalyzer<Arg>::to_string(str);
    NonSerializableAnalyzer<ArgT ...>::to_string(str);
  }

  static std::string to_string() {
    std::string s;
    to_string(s);
    return s;
  }
};

template<typename ... ArgT>
struct all_serializable: std::integral_constant<bool,
  sizeof...(ArgT) == 0 || (
    std::conjunction_v<traits::is_savable<ArgT> ...> &&
    std::conjunction_v<traits::is_loadable<ArgT>...>
  )
> {};

namespace impl {
template<typename>
struct AllMessageContentSerializable {
  using value = std::false_type;
};

template<typename ... ArgT>
struct AllMessageContentSerializable<std::tuple<ArgT ...>> {
  using value = all_serializable<ArgT ...>;
};

template<typename>
struct AllHandlerArgumentsSerializable {
  using value = std::false_type;
};

template<typename ... ArgT>
struct AllHandlerArgumentsSerializable<ArgumentsSignature<ArgT ...>> {
  using value = all_serializable<ArgT ...>;
};
} // namespace impl

// used in message.hpp
template<typename MsgCont>
using all_message_content_serializable =
typename impl::AllMessageContentSerializable<MsgCont>::value;

// used in message_handler.hpp
template<typename ArgsSign>
using all_handler_arguments_serializable =
typename impl::AllHandlerArgumentsSerializable<ArgsSign>::value;

} // namespace traits
} // namespace zaf
