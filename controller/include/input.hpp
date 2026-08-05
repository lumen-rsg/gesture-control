#pragma once

#include "config.hpp"
#include "gesture_state.hpp"


class InputController
{
  public:

    explicit InputController(const InputConfig& config);

    ~InputController();

    void update(const GestureState& state);

  private:

    int fd_;
    InputConfig config_;
    bool left_pressed_ = false;

    bool setup();

    void emit(
        int type,
        int code,
        int value
        );

    bool move(
        float x,
        float y
        );

    bool pressLeft();

    bool releaseLeft();
};
