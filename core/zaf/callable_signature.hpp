#include <cstddef>
#include <type_traits>

namespace zaf {
template<size_t N, typename Arg, typename ... ArgT>
struct ArgumentTypeAt {
  using type = typename ArgumentTypeAt<N - 1, ArgT ...>::type;
};

template<typename Arg, typename ... ArgT>
struct ArgumentTypeAt<0, Arg, ArgT ...> {
  using type = Arg;
};

template<typename ... ArgT>
struct ArgumentsSignature {
  constexpr static size_t size = sizeof ... (ArgT);

  template<size_t N>
  using arg_t = typename ArgumentTypeAt<N, ArgT ...>::type;

  template<size_t N>
  using decay_arg_t = std::decay_t<arg_t<N>>;
};

template<>
struct ArgumentsSignature<> {
  inline const static size_t size = 0;
};

template<typename RetType, typename ... ArgT>
struct CallableSignature : public std::true_type {
  using ret_t = RetType;
  using args_t = ArgumentsSignature<ArgT ...>;
};

namespace traits {
namespace impl {
// normal function
template<typename R, typename ... ArgT>
CallableSignature<R, ArgT ...> is_callable(R(*)(ArgT ...));

template<typename R, typename F, typename ... ArgT>
CallableSignature<R, ArgT ...> is_callable(R(F::*)(ArgT ...));

template<typename R, typename F, typename ... ArgT>
CallableSignature<R, ArgT ...> is_callable(R(F::*)(ArgT ...) const);

std::false_type is_callable(...);

template<typename F>
decltype(is_callable(&F::operator())) is_callable(F);
} // namespace impl

template<typename Callable>
using is_callable = decltype(impl::is_callable(std::declval<Callable>()));

} // namespace traits

} // namespace zaf
