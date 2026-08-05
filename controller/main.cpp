#include <iostream>

#include "ipc.hpp"
#include "input.hpp"
#include "config.hpp"
#include "recognizer.hpp"


int main()
{
  IPCServer server("/tmp/gesture.sock");

  Config config = Config::load("../config/config.json");

  InputController input(config.input);

  GestureRecognizer recognizer;

  gesture::Frame frame;

  while(server.receive(frame)) {
    GestureState state = recognizer.process(frame);

    input.update(state);

    std::cout
      << "Hand: "
      << state.hand_present
      << " X="
      << state.pointer_x
      << " Y="
      << state.pointer_y
      << " Pinch="
      << state.pinch
      << "\n";
  }

  return 0;
}
