#pragma once

#include <array>
#include <cstdint>

#include "protocol.hpp"

namespace gesture
{

  struct Landmark
  {
    float x;
    float y;
    float z;
  };

  struct Hand
  {
    std::array<Landmark, LANDMARK_COUNT> landmarks;
  };

  struct Frame
  {
    uint64_t timestamp;
    uint8_t hand_count;
    std::array<Hand, MAX_HANDS> hands;
  };

}
