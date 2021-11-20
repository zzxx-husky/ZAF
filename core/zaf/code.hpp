#pragma once

#include <memory>
#include <utility>

namespace zaf {
struct Code {
  size_t value;

  Code() = default;
  constexpr Code(size_t value) : value(value) {}

  inline operator size_t() const {
    return value;
  }
};

struct DefaultCodes {
  inline constexpr static size_t ZAFCodeBase = size_t(~0u) << (sizeof(size_t) << 2);
  inline constexpr static Code ForwardMessage           {ZAFCodeBase + 1};
  inline constexpr static Code SWSRMsgQueueRegistration {ZAFCodeBase + 2};
  inline constexpr static Code SWSRMsgQueueNotification {ZAFCodeBase + 3};
  inline constexpr static Code SWSRMsgQueueTermination  {ZAFCodeBase + 4};
  inline constexpr static Code SWSRMsgQueueConsumption  {ZAFCodeBase + 5};
};
} // namespace zaf
