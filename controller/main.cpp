#include <iostream>

#include "ipc.hpp"
#include "input.hpp"
#include "config.hpp"
#include "recognizer.hpp"
#include "protocol/inference.hpp"


int main()
{
  IPCServer server(5000);

  Config config = Config::load("../config/config.json");

  InputController input(config.input);

  GestureRecognizer recognizer;

  inference::Result result{};
  inference::Metrics metrics{};


  while(true) {
    IPCReceiveStatus status = server.receive( result, metrics);


    switch(status) {
      case IPCReceiveStatus::RESULT: {
          GestureState state = recognizer.process(result);

          input.update(state);

          std::cout
            << "Frame: "
            << result.frame_id
            << " | Class: "
            << result.classification.class_id
            << " | Confidence: "
            << result.classification.confidence
            << "\n";

          break;
        }


      case IPCReceiveStatus::METRICS: {
            std::cout
              << "Metrics"
              << " | FPS: "
              << metrics.fps
         
              << " | Inference: "
              << metrics.inference_time_us
              << " us"

              << " | Transfer: "
              << metrics.transfer_time_us
              << " us"

              << " | Total: "
              << metrics.total_latency_us
              << " us"

              << " | CPU: "
              << metrics.cpu_usage
              << "%"

              << " | Memory: "
              << metrics.memory_usage
              << "%"

              << " | Temperature: "
              << metrics.temperature
              << " C"

              << "\n";

          break;
        }


      case IPCReceiveStatus::DISCONNECTED:

        std::cerr << "Vision module disconnected\n";

        return 1;


      case IPCReceiveStatus::ERROR:

        std::cerr << "IPC error\n";

        return 1;
    }
  }


  return 0;
}
