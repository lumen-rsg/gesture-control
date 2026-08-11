#pragma once

#include <cstdint>


namespace inference
{
  constexpr uint32_t PROTOCOL_VERSION = 2;

  enum class ResultType : uint8_t
  {
    CLASSIFICATION = 0,
    DETECTION = 1
  };

  enum class MessageType : uint8_t
  {
    RESULT = 0,
    METRICS = 1
  };
}
