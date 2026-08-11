#include "recognizer.hpp"


GestureState GestureRecognizer::process(const inference::Result& result) const
{
  GestureState state;

  if(!result.hand_present)
    return state;

  const auto& classification = result.classification;

  if(classification.class_id == NO_CLASSIFICATION)
    return state;

  if(classification.confidence < MIN_CONFIDENCE)
    return state;

  state.hand_present = true;

  state.confidence = classification.confidence;

  state.hand_x = result.hand.x;

  state.hand_y = result.hand.y;

  state.hand_width = result.hand.width;

  state.hand_height = result.hand.height;

  switch(classification.class_id)
  {
    case FIST_CLASS:
      state.gesture = GestureType::FIST;
      break;

    case OPEN_HAND_CLASS:
      state.gesture = GestureType::OPEN_HAND;
      break;

    default:
      state.gesture = GestureType::NONE;
      break;
  }

  return state;
}
