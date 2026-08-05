#include "recognizer.hpp"

#include <cmath>

GestureState GestureRecognizer::process(const gesture::Frame& frame) const
{
  GestureState state;

  if(frame.hand_count < 1)
    return state;

  state.hand_present = true;

  const auto& hand = frame.hands[0];

  const auto& index = hand.landmarks[INDEX_TIP];

  const auto& thumb = hand.landmarks[THUMB_TIP];

  state.pointer_x = index.x;
  state.pointer_y = index.y;

  state.pinch_distance =
    distance(index, thumb);

  state.pinch =
    state.pinch_distance <
    PINCH_THRESHOLD;

  return state;
}

float GestureRecognizer::distance(
    const gesture::Landmark& a,
    const gesture::Landmark& b
    ) const
{
  const float dx = a.x - b.x;
  const float dy = a.y - b.y;
  const float dz = a.z - b.z;

  return std::sqrt(
      dx * dx +
      dy * dy +
      dz * dz
      );
}
