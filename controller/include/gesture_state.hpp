#pragma once

#include <cstdint>


enum class GestureType : uint8_t
{
  NONE = 0,
  PALM,
  FIST,
  OK,
  ONE
};


struct GestureState
{
  bool hand_present = false;

  GestureType gesture = GestureType::NONE;

  float confidence = 0.0f;

  //
  // Normalized hand bounding box.
  //
  float hand_x = 0.0f;
  float hand_y = 0.0f;

  float hand_width = 0.0f;
  float hand_height = 0.0f;
};
