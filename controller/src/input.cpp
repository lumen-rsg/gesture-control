#include "input.hpp"

#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

#include <linux/uinput.h>

#include <iostream>


InputController::InputController(const InputConfig& config) :
  fd_(-1),
  config_(config)
{
  setup();
}


InputController::~InputController()
{
  if(fd_ >= 0) {
    if(left_pressed_)
      releaseLeft();

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
  // Event types
  //

  ioctl(fd_, UI_SET_EVBIT, EV_KEY);

  ioctl(fd_, UI_SET_EVBIT, EV_ABS);

  //
  // Left mouse button
  //

  ioctl(fd_, UI_SET_KEYBIT, BTN_LEFT);

  //
  // Absolute cursor coordinates
  //

  ioctl(fd_, UI_SET_ABSBIT, ABS_X);

  ioctl(fd_, UI_SET_ABSBIT, ABS_Y);

  //
  // Virtual device
  //

  uinput_user_dev device{};

  snprintf(device.name, UINPUT_MAX_NAME_SIZE, "Gesture Virtual Mouse");

  device.id.bustype = BUS_USB;
  device.id.vendor  = 0x1234;
  device.id.product = 0x5678;
  device.id.version = 1;


  //
  // Coordinate range.
  //
  // We use the full 16-bit range of the
  // virtual absolute mouse.
  //

  device.absmin[ABS_X] = 0;
  device.absmax[ABS_X] = 32767;

  device.absmin[ABS_Y] = 0;
  device.absmax[ABS_Y] = 32767;


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
    int value
    )
{
  input_event event{};

  event.type = type;
  event.code = code;
  event.value = value;

  write(fd_, &event, sizeof(event));
}


bool InputController::move(float x, float y)
{
  if(fd_ < 0)
    return false;


  //
  // Clamp normalized coordinates.
  //

  x = std::clamp(x, 0.0f, 1.0f);

  y = std::clamp(y, 0.0f, 1.0f);


  //
  // Coordinate inversion.
  //

  if(config_.invert_x)
    x = 1.0f - x;

  if(config_.invert_y)
    y = 1.0f - y;


  //
  // Convert normalized coordinates
  // to uinput absolute coordinates.
  //

  constexpr int MAX_COORD = 32767;

  int px = static_cast<int>(x * MAX_COORD);

  int py = static_cast<int>(y * MAX_COORD);

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


void InputController::update(const GestureState& state)
{
  //
  // No hand.
  //
  // Stop dragging and don't move cursor.
  //

  if(!state.hand_present) {
    if(left_pressed_) {
      releaseLeft();
      left_pressed_ = false;
    }

    return;
  }


  //
  // Use center of the hand bounding box
  // as cursor position.
  //

  const float pointer_x = state.hand_x + state.hand_width * 0.5f;

  const float pointer_y = state.hand_y + state.hand_height * 0.5f;

  move(pointer_x, pointer_y);


  //
  // Gesture interpretation.
  //

  switch(state.gesture) {
    case GestureType::FIST:
      if(!left_pressed_) {
        if(pressLeft())
          left_pressed_ = true;
      }
      break;

    case GestureType::OPEN_HAND:
      if(left_pressed_) {
        if(releaseLeft())
          left_pressed_ = false;
      }
      break;


    case GestureType::NONE:
      break;
  }
}
