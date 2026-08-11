#include <iostream>

#include "config.hpp"
#include "video.hpp"
#include "dashboard/dashboard.hpp"
#include "input.hpp"
#include "ipc.hpp"
#include "protocol/inference.hpp"
#include "recognizer.hpp"


int main()
{
  //
  // IPC
  //

  IPCServer server(5000);


  //
  // Configuration
  //

  Config config = Config::load("../config/config.json");

  VideoServer video_server(config.dashboard.video_port);

  if(config.dashboard.camera_preview) {
    if(!video_server.start()) {
      std::cerr << "Failed to start video server\n";
    }
  }

  //
  // Input
  //

  InputController input(config.input);


  //
  // Gesture recognition
  //

  GestureRecognizer recognizer;


  //
  // Dashboard
  //

  Dashboard dashboard;

  if(!dashboard.initialize()) {

    std::cerr << "Failed to initialize dashboard\n";

    return 1;
  }

  dashboard.set_video_server(&video_server);

  //
  // IPC state
  //

  inference::Result result{};
  inference::Metrics metrics{};

  IPCServer::MessageType message_type;


  bool have_result = false;
  bool have_metrics = false;


  //
  // Main loop
  //

  while(!dashboard.should_close()) {

    if(!server.receive(
          message_type,
          result,
          metrics
          ))
    {
      std::cerr << "IPC error\n";

      break;
    }


    //
    // RESULT
    //

    if(message_type == IPCServer::MessageType::RESULT) {

      have_result = true;


      //
      // Gesture recognition and input control
      //

      GestureState state = recognizer.process(result);

      input.update(state);


      //
      // Do not update dashboard yet.
      //
      // We want RESULT and METRICS belonging
      // to the same frame.
      //

    }


    //
    // METRICS
    //

    else if( message_type == IPCServer::MessageType::METRICS) {
      have_metrics = true;
    }


    //
    // Complete frame
    //

    if(have_result && have_metrics) {

      if(result.frame_id == metrics.frame_id) {

        dashboard.update(result, metrics);

        have_result = false;
        have_metrics = false;
      }
      else {

        //
        // This should never happen with our
        // current Vision implementation.
        //

        std::cerr
          << "Frame ID mismatch: "
          << result.frame_id
          << " != "
          << metrics.frame_id
          << "\n";


        //
        // Keep the newer frame and wait
        // for its corresponding message.
        //

        if(result.frame_id > metrics.frame_id) {
          have_metrics = false;
        } else {
          have_result = false;
        }
      }
    }


    //
    // Render GUI.
    //
    // This is currently reached after every
    // IPC message.
    //

    dashboard.render();
  }


  //
  // Cleanup
  //

  dashboard.shutdown();

  return 0;
}
