#include "input.hpp"

#include <fcntl.h>
#include <unistd.h>

#include <linux/uinput.h>

#include <cstring>
#include <iostream>


InputController::InputController(const InputConfig& config) : fd_(-1), config_(config)
{
  setup();
}

InputController::~InputController()
{
  if(fd_ >= 0) {
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

  // Поддержка абсолютных координат
  ioctl(fd_, UI_SET_EVBIT, EV_KEY);

  ioctl(fd_, UI_SET_EVBIT, EV_ABS);

  // Левая кнопка мыши
  ioctl(fd_, UI_SET_KEYBIT, BTN_LEFT);

  // Координаты
  ioctl(fd_, UI_SET_ABSBIT, ABS_X);

  ioctl(fd_, UI_SET_ABSBIT, ABS_Y);

  uinput_user_dev device{};

  snprintf(
      device.name,
      UINPUT_MAX_NAME_SIZE,
      "Gesture Virtual Mouse"
      );

  device.id.bustype = BUS_USB;
  device.id.vendor  = 0x1234;
  device.id.product = 0x5678;
  device.id.version = 1;

  device.absmin[ABS_X] = 0;
  device.absmax[ABS_X] = 32767;

  device.absmin[ABS_Y] = 0;
  device.absmax[ABS_Y] = 32767;

  write(fd_, &device, sizeof(device));

  ioctl(fd_, UI_DEV_CREATE);

  std::cout << "Virtual mouse created\n";

  return true;
}

void InputController::emit(int type, int code, int value)
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

  if(config_.invert_x)
    x = 1.0f - x;

  if(config_.invert_y)
    y = 1.0f - y;

  int px = static_cast<int>(x * config_.screen_width);

  int py = static_cast<int>(y * config_.screen_height);

  emit(EV_ABS, ABS_X, px);

  emit(EV_ABS, ABS_Y, py);

  emit(EV_SYN, SYN_REPORT, 0);

  return true;
}

bool InputController::pressLeft()
{
  emit(EV_KEY, BTN_LEFT, 1);

  emit(EV_SYN, SYN_REPORT, 0);

  return true;
}

bool InputController::releaseLeft()
{
  emit(EV_KEY, BTN_LEFT, 0);

  emit(EV_SYN, SYN_REPORT, 0);

  return true;
}

void InputController::update(const GestureState& state)
{
    if(!state.hand_present)
    {
        if(left_pressed_)
        {
            releaseLeft();
            left_pressed_ = false;
        }

        return;
    }


    move(
        state.pointer_x,
        state.pointer_y
    );


    if(state.pinch && !left_pressed_)
    {
        pressLeft();

        left_pressed_ = true;
    }


    if(!state.pinch && left_pressed_)
    {
        releaseLeft();

        left_pressed_ = false;
    }
}
