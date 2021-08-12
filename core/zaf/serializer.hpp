#pragma once

#include <cstring>
#include <tuple>
#include <type_traits>
#include <vector>

#include "callable_signature.hpp"
#include "traits.hpp"
#include "zaf_exception.hpp"

namespace zaf {
class Serializer {
public:
  Serializer(std::vector<char>& byte_buffer);

  inline void write_bytes(const char* b, size_t n) {
    bytes.insert(bytes.end(), b, b + n);
  }

  template<typename I>
  inline void write_pod(I&& x) {
    write_bytes(reinterpret_cast<const char*>(&x), sizeof(x));
  }

  Serializer& write();

  template<typename T, typename ... Ts>
  Serializer& write(T&& t, Ts&& ... ts);

  inline const std::vector<char>& get_underlying_bytes() const {
    return bytes;
  }

private:
  std::vector<char>& bytes;
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

  template<typename T>
  Deserializer& read(T& t);

private:
  const char* offset;
};
} // namespace zaf

#include "serialization.hpp"

namespace zaf {
template<typename T, typename ... Ts>
Serializer& Serializer::write(T&& t, Ts&& ... ts) {
  serialize(*this, std::forward<T>(t));
  write(std::forward<Ts>(ts)...);
  return *this;
}

template<typename T>
T Deserializer::read() {
  return deserialize<T>(*this);
}

template<typename T>
Deserializer& Deserializer::read(T& t) {
  deserialize<T>(*this, t);
  return *this;
}

namespace traits {
template<typename ... ArgT>
using all_serializable = std::integral_constant<bool,
  sizeof...(ArgT) == 0 || (std::conjunction_v<traits::is_savable<ArgT> ...> && std::conjunction_v<traits::is_loadable<ArgT>...>)
>;

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
