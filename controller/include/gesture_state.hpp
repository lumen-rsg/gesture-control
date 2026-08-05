#pragma once

struct GestureState
{
    bool hand_present = false;

    float pointer_x = 0.0f;
    float pointer_y = 0.0f;
    float pinch_distance = 0.0f;
    bool pinch = false;
};
