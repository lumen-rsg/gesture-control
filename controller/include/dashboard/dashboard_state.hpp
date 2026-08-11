#pragma once

#include <cstddef>
#include <vector>

#include "protocol/inference.hpp"


struct DashboardState
{
  inference::Result result{};

  inference::Metrics metrics{};

  bool connected = false;

  std::vector<float> latency_history;

  std::vector<float> fps_history;
};
