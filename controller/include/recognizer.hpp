#pragma once

#include "protocol/gesture.hpp"
#include "gesture_state.hpp"

class GestureRecognizer
{
  public:
    GestureState process(const gesture::Frame& frame) const;

  private:
    static constexpr std::size_t THUMB_TIP = 4;
    static constexpr std::size_t INDEX_TIP = 8;
    float pinch_distance = 0.0f;
    static constexpr float PINCH_THRESHOLD = 0.05f;

    float distance(
        const gesture::Landmark& a,
        const gesture::Landmark& b
        ) const;
};
