#pragma once
#include "message_handler.hpp"

namespace zaf {
struct Code {
  const size_t value;

  constexpr Code(size_t value) : value(value) {}

  // create a pair that links the code with the user_handler
  // while the type of the user_handler is erased.
  template<typename Handler>
  inline friend auto operator-(Code code, Handler&& user_handler) {
    // return as it is
    using HandlerX = traits::remove_cvref_t<Handler>;
    MessageHandler* handler = new TypedMessageHandler<HandlerX>(std::forward<Handler>(user_handler));
    return std::make_pair(code.value, std::unique_ptr<MessageHandler>(handler));
  }

  inline operator size_t() const {
    return value;
  }
};

struct DefaultCodes {
  constexpr static size_t ZAFCodeBase = size_t(~0u) << (sizeof(size_t) << 2);
  constexpr static Code ForwardMessage           {ZAFCodeBase + 1};
  constexpr static Code SWSRMsgQueueRegistration {ZAFCodeBase + 2};
  constexpr static Code SWSRMsgQueueNotification {ZAFCodeBase + 3};
  constexpr static Code SWSRMsgQueueTermination  {ZAFCodeBase + 4};
  constexpr static Code SWSRMsgQueueConsumption  {ZAFCodeBase + 5};
};
} // namespace zaf
