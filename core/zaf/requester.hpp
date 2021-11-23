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

// This seems to be a hack, i.e., need to know the details of Request
static unsigned get_request_id(const Message& m) {
  auto& body = m.get_body();
  if (body.get_code() != DefaultCodes::Request) {
    throw ZAFException("Attempt to get request id from a non-request message. "
      "Actual code of the message: ", body.get_code());
  }
  if (body.is_serialized()) {
    return static_cast<const SerializedMessageBody&>(body)
      .make_deserializer()
      .read<unsigned>();
  } else {
    std::vector<std::uintptr_t> ptrs(2);
    static_cast<const MemoryMessageBody&>(body).get_element_ptrs(ptrs);
    return *reinterpret_cast<unsigned*>(ptrs[0]);
  }
}

} // namespace zaf
