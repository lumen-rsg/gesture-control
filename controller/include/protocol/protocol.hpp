#pragma once

#include <cstddef>
#include <cstdint>

namespace gesture
{

  constexpr uint32_t PROTOCOL_VERSION = 1;

  constexpr std::size_t LANDMARK_COUNT = 21;
  constexpr std::size_t MAX_HANDS = 2;

  constexpr std::size_t LANDMARK_FLOATS = 3;

}
