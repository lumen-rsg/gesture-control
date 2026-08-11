#pragma once

#include "gesture_state.hpp"
#include "protocol/inference.hpp"

class GestureRecognizer
{
  public:

    GestureState process(const inference::Result& result) const;

  private:

    static constexpr uint32_t FIST_CLASS = 0;

    static constexpr uint32_t OPEN_HAND_CLASS = 1;

    static constexpr uint32_t NO_CLASSIFICATION = 0xFFFFFFFF;

    static constexpr float MIN_CONFIDENCE = 0.5f;
};
