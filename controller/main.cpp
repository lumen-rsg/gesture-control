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


  //
  // Video server
  //

  VideoServer video_server( config.dashboard.video_port);

  if(config.dashboard.camera_preview) {
    if(!video_server.start()) {
      std::cerr << "Failed to start video server\n";
    }
  }


  //
  // Input
  //

  InputController input( config.input);


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

  dashboard.set_video_server( &video_server);

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
    if(!server.receive( message_type, result, metrics)) {
      std::cerr << "IPC error\n";
      break;
    }


    //
    // RESULT
    //

    if(message_type == IPCServer::MessageType::RESULT) {
      have_result = true;


      //
      // Interpret neural network result.
      //

      GestureState state = recognizer.process(result);


      //
      // Apply gesture to input device.
      //

      input.update(state);
    }


    //
    // METRICS
    //

    else if(message_type == IPCServer::MessageType::METRICS) {
      have_metrics = true;
    }


    //
    // Complete frame
    //

    if(have_result && have_metrics) {
      if(result.frame_id == metrics.frame_id) {
        dashboard.update( result, metrics);

        have_result = false;
        have_metrics = false;
      } else {
        std::cerr << "Frame ID mismatch: " << result.frame_id << " != " << metrics.frame_id << "\n";


        //
        // Keep the newer frame.
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

    dashboard.render();
  }


  //
  // Cleanup
  //

  dashboard.shutdown();

  return 0;
}
