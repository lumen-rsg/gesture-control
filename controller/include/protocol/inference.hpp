#pragma once

#include <cstdint>

#include "protocol.hpp"


namespace inference
{

  struct Classification
  {
    uint32_t class_id;
    float confidence;
  };


  struct HandPosition
  {
    //
    // Normalized coordinates [0, 1].
    //
    // x/y are the palm center produced by HandMapper.
    //

    float x;
    float y;

    //
    // Currently unused by HandMapper,
    // kept in the protocol for compatibility.
    //

    float width;
    float height;
  };


  struct Result
  {
    uint64_t frame_id;
    uint64_t timestamp;

    uint8_t hand_present;

    Classification classification;

    HandPosition hand;
  };


  struct Metrics
  {
    uint64_t frame_id;
    uint64_t timestamp;

    float fps;

    uint32_t capture_time_us;
    uint32_t preprocess_time_us;
    uint32_t inference_time_us;
    uint32_t transfer_time_us;
    uint32_t postprocess_time_us;
    uint32_t total_latency_us;

    float cpu_usage;
    float memory_usage;
    float temperature;
  };

}
