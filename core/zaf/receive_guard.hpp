#pragma once

namespace zaf {
template<typename Receive>
inline bool try_receive_guard(Receive&& receive) {
  // in case `recv` is broken by EINTR, retry
  // for any exception which is NOT EINTR zmq::error_t, throw it
  try {
    receive();
    return true;
  } catch (const zmq::error_t& e) {
    if (e.num() != EINTR) { throw; }
    return false;
  }
}

template<typename Receive>
inline void receive_guard(Receive&& receive) {
  while (!try_receive_guard(receive));
}
} // namespace zaf
