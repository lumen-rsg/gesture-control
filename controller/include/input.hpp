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
    bool right_pressed_ = false;

    //
    // EMA-filtered hand position.
    //
    bool position_initialized_ = false;

    float smooth_x_ = 0.0f;
    float smooth_y_ = 0.0f;

    //
    // Previous filtered hand position.
    //
    float previous_hand_x_ = 0.0f;
    float previous_hand_y_ = 0.0f;

    //
    // Relative mouse position.
    //
    float mouse_x_ = 0.5f;
    float mouse_y_ = 0.5f;

    //
    // Scrolling.
    //
    bool scrolling_ = false;

    float scroll_start_x_ = 0.0f;
    float scroll_start_y_ = 0.0f;

    float last_scroll_x_ = 0.0f;
    float last_scroll_y_ = 0.0f;


    bool setup();

    void emit(int type, int code, float value);

    bool move(float x, float y);

    bool pressLeft();

    bool releaseLeft();

    bool pressRight();

    bool releaseRight();

    void startScroll(float x, float y);

    void updateScroll(float x, float y);

    void stopScroll();

    void releaseButtons();

    float smooth(float value, float previous);

    static constexpr float POSITION_ALPHA = 0.4f;

    static constexpr float SCROLL_THRESHOLD = 0.01f;
};
