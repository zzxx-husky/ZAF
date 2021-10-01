// To be included inside serializer.hpp

namespace zaf {
// 1. Serialization for POD
template<typename POD,
  typename RAW = traits::remove_cvref_t<POD>,
  std::enable_if_t<std::is_pod<RAW>::value>* = nullptr,
  std::enable_if_t<!std::is_pointer<RAW>::value>* = nullptr>
void serialize(Serializer& s, POD&& pod) {
  s.write_pod(std::forward<POD>(pod));
}

template<typename POD,
  std::enable_if_t<std::is_pod<POD>::value>* = nullptr,
  std::enable_if_t<!std::is_pointer<POD>::value>* = nullptr>
void deserialize(Deserializer& s, POD& pod) {
  s.read_pod(pod);
}

namespace traits {
namespace impl {
template<typename Arg>
auto is_savable(int) -> decltype(
  serialize(std::declval<Serializer&>(), std::declval<Arg&>()),
  std::true_type{}
);

template<typename>
std::false_type is_savable(...);

template<typename Arg>
auto is_inplace_loadable(int) -> decltype(
  deserialize(std::declval<Deserializer&>(), std::declval<Arg&>()),
  std::true_type{}
);

template<typename>
std::false_type is_inplace_loadable(...);

template<typename Arg>
auto is_loadable(int) -> decltype(
  deserialize<Arg>(std::declval<Deserializer&>()),
  std::true_type{}
);

template<typename>
std::false_type is_loadable(...);
} // namespace impl

template<typename T>
using is_savable = decltype(impl::is_savable<traits::remove_cvref_t<T>>(0));

template<typename T>
using is_inplace_loadable = decltype(impl::is_inplace_loadable<traits::remove_cvref_t<T>>(0));

template<typename T>
using is_loadable = decltype(impl::is_loadable<traits::remove_cvref_t<T>>(0));
} // namespace traits

// 2. Serialization for pointers 
// a pointer is savable if the value is savable
template<typename P,
  typename RAW = traits::remove_cvref_t<P>,
  std::enable_if_t<std::is_pointer<RAW>::value>* = nullptr,
  typename V = traits::remove_cvref_t<decltype(*std::declval<P>())>,
  std::enable_if_t<traits::is_savable<V>::value>* = nullptr>
inline void serialize(Serializer& s, P&& pnt) {
  serialize(s, *pnt);
}

// a pointer is loadable if the value is loadable
template<typename P,
  std::enable_if_t<std::is_pointer<P>::value>* = nullptr,
  typename V = traits::remove_cvref_t<decltype(*std::declval<P>())>,
  std::enable_if_t<traits::is_loadable<V>::value>* = nullptr>
void deserialize(Deserializer& s, P& pnt) {
  deserialize(s, *pnt);
}

template<typename T,
  std::enable_if_t<std::is_default_constructible<T>::value>* = nullptr,
  std::enable_if_t<!std::is_pointer<T>::value>* = nullptr,
  std::enable_if_t<traits::is_inplace_loadable<T>::value>* = nullptr>
T deserialize(Deserializer& s) {
  T t{};
  deserialize(s, t);
  return t;
}

template<typename T,
  std::enable_if_t<std::is_pointer<T>::value>* = nullptr,
  // remove reference in the value type
  typename V = traits::remove_cvref_t<decltype(*std::declval<T>())>,
  std::enable_if_t<std::is_default_constructible<V>::value>* = nullptr,
  std::enable_if_t<traits::is_inplace_loadable<V>::value>* = nullptr>
T deserialize(Deserializer& s) {
  T t = new V{};
  deserialize(s, t);
  return t;
}

// 3. Serialization for iterable, i.e., containers 
template<typename Iterable,
  std::enable_if_t<traits::is_iterable<Iterable>::value>* = nullptr,
  typename E = traits::remove_cvref_t<decltype(*std::declval<Iterable&>().begin())>,
  std::enable_if_t<traits::is_savable<E>::value>* = nullptr>
void serialize(Serializer& s, Iterable&& i) {
  serialize(s, size_t(i.size()));
  for (auto&& x : i) {
    serialize(s, x);
  }
}

template<typename Iterable,
  std::enable_if_t<traits::is_iterable<Iterable>::value>* = nullptr,
  typename E = traits::remove_cvref_t<decltype(*std::declval<Iterable&>().begin())>,
  std::enable_if_t<traits::is_loadable<E>::value>* = nullptr>
void deserialize(Deserializer& s, Iterable& it) {
  auto size = deserialize<size_t>(s);
  if constexpr (traits::has_reserve<Iterable>::value) {
    it.reserve(size);
  }
  for (size_t i = 0; i < size; i++) {
    if constexpr (traits::has_emplace_back<Iterable, E>::value) {
      it.emplace_back(deserialize<E>(s));
    } else if constexpr (traits::has_emplace<Iterable, E>::value) {
      it.emplace(deserialize<E>(s));
    } else if constexpr (traits::has_insert<Iterable, E>::value) {
      it.insert(deserialize<E>(s));
    } else {
      throw ZAFException("Failed to deserialize items from the Iterable object:"
        " Possibly the Iterable object does not provide emplace_back, empalce or insert method for insertion."
        " Or the item is not deserializable");
    }
  }
}

// 4. Serialization for std::pair
template<typename A, typename B>
void serialize(Serializer& s, const std::pair<A, B>& p) {
  serialize(s, p.first);
  serialize(s, p.second);
}

template<typename P,
  std::enable_if_t<traits::is_pair<P>::value>* = nullptr,
  typename A = typename traits::is_pair<P>::first_type,
  typename B = typename traits::is_pair<P>::second_type>
std::pair<A, B> deserialize(Deserializer& s) {
  auto a = deserialize<traits::remove_cvref_t<A>>(s);
  auto b = deserialize<traits::remove_cvref_t<B>>(s);
  return {
    std::forward<decltype(a)>(a),
    std::forward<decltype(b)>(b)
  };
}

// 5. Serialization for std::string to write all the bytes together
inline void serialize(Serializer& s, const std::string& str) {
  s.write_pod(size_t(str.size()));
  s.write_bytes(str.data(), str.size());
}

inline void deserialize(Deserializer& s, std::string& str) {
  str.resize(s.read<size_t>());
  s.read_bytes(str.data(), str.size());
}
} // namespace zaf
