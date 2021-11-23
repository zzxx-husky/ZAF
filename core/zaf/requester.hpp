// To be included inside actor_behavior.hpp

namespace zaf {
// make all `send` invocations to `request`
class Requester {
public:
  ActorBehavior& self;

  template<typename ... ArgT>
  decltype(auto) send(ArgT&& ... args);

  template<typename ... ArgT>
  decltype(auto) request(ArgT&& ... args);
};

template<typename ... ArgT>
decltype(auto) Requester::send(ArgT&& ... args) {
  return self.request(std::forward<ArgT>(args) ...);
}

template<typename ... ArgT>
decltype(auto) Requester::request(ArgT&& ... args) {
  return self.request(std::forward<ArgT>(args) ...);
}

} // namespace zaf
