#include "input.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <linux/uinput.h>

#include <algorithm>
#include <cmath>
#include <iostream>


InputController::InputController( const InputConfig& config) : fd_(-1), config_(config)
{
  setup();
}


InputController::~InputController()
{
  if(fd_ >= 0) {

    releaseButtons();

    if(scrolling_)
      stopScroll();

    ioctl(fd_, UI_DEV_DESTROY);

    close(fd_);
  }
}


bool InputController::setup()
{
  fd_ = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

  if(fd_ < 0) {
    perror("open /dev/uinput");
    return false;
  }

  //
  // Mouse buttons.
  //

  ioctl(fd_, UI_SET_EVBIT, EV_KEY);

  ioctl(fd_, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(fd_, UI_SET_KEYBIT, BTN_RIGHT);


  //
  // Absolute mouse position.
  //

  ioctl(fd_, UI_SET_EVBIT, EV_ABS);

  ioctl(fd_, UI_SET_ABSBIT, ABS_X);
  ioctl(fd_, UI_SET_ABSBIT, ABS_Y);


  //
  // Mouse wheel.
  //

  ioctl(fd_, UI_SET_EVBIT, EV_REL);
  ioctl(fd_, UI_SET_RELBIT, REL_WHEEL);


  //
  // Virtual device.
  //

  uinput_user_dev device{};

  snprintf(device.name, UINPUT_MAX_NAME_SIZE, "Gesture Virtual Mouse");

  device.id.bustype = BUS_USB;
  device.id.vendor  = 0x1234;
  device.id.product = 0x5678;
  device.id.version = 1;


  device.absmin[ABS_X] = 0;
  device.absmax[ABS_X] = config_.screen_width;

  device.absmin[ABS_Y] = 0;
  device.absmax[ABS_Y] = config_.screen_height;


  if(write(fd_, &device, sizeof(device)) < 0) {
    perror("write uinput device");
    close(fd_);
    fd_ = -1;
    return false;
  }


  if(ioctl(fd_, UI_DEV_CREATE) < 0) {
    perror("UI_DEV_CREATE");
    close(fd_);
    fd_ = -1;
    return false;
  }

  std::cout << "Virtual mouse created\n";

  return true;
}


void InputController::emit(
    int type,
    int code,
    float value
    )
{
  if(fd_ < 0)
    return;


  input_event event{};

  event.type = type;
  event.code = code;
  event.value = static_cast<int>(value);

  write(fd_, &event, sizeof(event));
}


bool InputController::move(float x, float y)
{
  if(fd_ < 0)
    return false;


  //
  // Input coordinates are already normalized
  // HandMapper coordinates.
  //
  // x/y represent the palm position directly.
  //


  x = std::clamp(x, 0.0f, 1.0f);
  y = std::clamp(y, 0.0f, 1.0f);


  //
  // Apply axis inversion before smoothing.
  //

  if(config_.invert_x)
    x = 1.0f - x;

  if(config_.invert_y)
    y = 1.0f - y;


  //
  // Initialize EMA.
  //

  if(!position_initialized_) {

    smooth_x_ = x;
    smooth_y_ = y;

    position_initialized_ = true;

  } else {

    smooth_x_ = smooth(x, smooth_x_);

    smooth_y_ = smooth(y, smooth_y_);
  }


  //
  // Convert normalized coordinates
  // directly into absolute mouse coordinates.
  //

  const float px = smooth_x_ * config_.screen_width;

  const float py = smooth_y_ * config_.screen_height;


  emit(EV_ABS, ABS_X, px);

  emit(EV_ABS, ABS_Y, py);

  emit(EV_SYN, SYN_REPORT, 0);

  return true;
}


bool InputController::pressLeft()
{
  if(fd_ < 0)
    return false;

  emit(EV_KEY, BTN_LEFT, 1);

  emit(EV_SYN, SYN_REPORT, 0);

  return true;
}


bool InputController::releaseLeft()
{
  if(fd_ < 0)
    return false;

  emit(EV_KEY, BTN_LEFT, 0);

  emit(EV_SYN, SYN_REPORT, 0);

  return true;
}


bool InputController::pressRight()
{
  if(fd_ < 0)
    return false;


  emit(EV_KEY, BTN_RIGHT, 1);

  emit(EV_SYN, SYN_REPORT, 0);


  return true;
}


bool InputController::releaseRight()
{
  if(fd_ < 0)
    return false;


  emit(EV_KEY, BTN_RIGHT, 0);

  emit(EV_SYN, SYN_REPORT, 0);

  return true;
}


void InputController::startScroll(float x, float y)
{
  scrolling_ = true;

  scroll_start_x_ = x;
  scroll_start_y_ = y;

  last_scroll_x_ = x;
  last_scroll_y_ = y;
}


void InputController::updateScroll(float x, float y)
{
  if(!scrolling_)
    return;


  const float dx = x - last_scroll_x_;

  const float dy = y - last_scroll_y_;


  //
  // Ignore tiny movements.
  //

  if( std::abs(dx) < SCROLL_THRESHOLD && std::abs(dy) < SCROLL_THRESHOLD) {
    return;
  }


  //
  // Vertical movement controls the wheel.
  //
  // Hand moves UP   -> scroll UP
  // Hand moves DOWN -> scroll DOWN
  //

  if(std::abs(dy) >= SCROLL_THRESHOLD) {

    const int direction = dy < 0.0f ? 1 : -1;


    emit(EV_REL, REL_WHEEL, direction);

    emit(EV_SYN, SYN_REPORT, 0); 
  
  }


  last_scroll_x_ = x;
  last_scroll_y_ = y;
}


void InputController::stopScroll()
{
  scrolling_ = false;
}


void InputController::releaseButtons()
{
  if(left_pressed_) {

    releaseLeft();

    left_pressed_ = false;
  }


  if(right_pressed_) {

    releaseRight();

    right_pressed_ = false;
  }
}


void InputController::update(const GestureState& state)
{
  //
  // No hand.
  //

  if(!state.hand_present) {
    if(left_pressed_) {
      releaseLeft();
      left_pressed_ = false;
    }

    if(right_pressed_) {
      releaseRight();
      right_pressed_ = false;
    }


    stopScroll();

    //
    // The next detected hand will establish
    // a fresh cursor position.
    //

    position_initialized_ = false;

    return;
  }


  //
  // HandMapper already calculated the palm center.
  //
  // DO NOT add width/2 or height/2 here.
  //

  const float hand_x = state.hand_x;

  const float hand_y = state.hand_y;


  switch(state.gesture)
  {
    //
    // PALM
    //
    // Normal cursor movement.
    //

    case GestureType::PALM:
      {
        stopScroll();


        if(right_pressed_) {

          releaseRight();

          right_pressed_ = false;
        }


        if(left_pressed_) {

          releaseLeft();

          left_pressed_ = false;
        }

        move(hand_x, hand_y);

        break;
      }


      //
      // FIST
      //
      // Hold left mouse button.
      //

    case GestureType::FIST:
      {
        stopScroll();


        if(right_pressed_) {

          releaseRight();

          right_pressed_ = false;
        }

        move(hand_x, hand_y);


        if(!left_pressed_) {

          pressLeft();

          left_pressed_ = true;
        }


        break;
      }


      //
      // OK
      //
      // Start / continue scrolling.
      //

    case GestureType::OK:
      {
        if(left_pressed_) {

          releaseLeft();

          left_pressed_ = false;
        }


        if(right_pressed_) {

          releaseRight();

          right_pressed_ = false;
        }


        if(!scrolling_) {
          startScroll(hand_x, hand_y);
        } else {
          updateScroll(hand_x, hand_y);
        }

        break;
      }


      //
      // ONE
      //
      // Right mouse button.
      //

    case GestureType::ONE:
      {
        stopScroll();


        if(left_pressed_) {

          releaseLeft();

          left_pressed_ = false;
        }


        move(hand_x, hand_y);


        if(!right_pressed_) {

          pressRight();

          right_pressed_ = true;
        }


        break;
      }


      //
      // Unknown gesture.
      //

    case GestureType::NONE:
      {
        stopScroll();

        releaseButtons();

        break;
      }
  }
}


float InputController::smooth(float value, float previous)
{
  return POSITION_ALPHA * value + (1.0f - POSITION_ALPHA) * previous; 
}
