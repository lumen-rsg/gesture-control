#include "config.hpp"

#include <fstream>
#include <iostream>


Config Config::load(const std::string& path)
{
  Config config;

  std::ifstream file(path);

  if(!file) {
    std::cerr << "Cannot open config: " << path << "\n";

    return config;
  }

  json data;

  file >> data;

  load_input_config(data, config);

  load_dashboard_config(data, config);

  return config;
}

void Config::load_input_config(json& data_, Config& config_)
{
  if(!data_.contains("input"))
    return;

  const auto& input = data_["input"];

  if(input.contains("invert_x"))
    config_.input.invert_x = input["invert_x"];

  if(input.contains("invert_y"))
    config_.input.invert_y = input["invert_y"];

  if(input.contains("screen_width"))
    config_.input.screen_width = input["screen_width"];

  if(input.contains("screen_height"))
    config_.input.screen_height = input["screen_height"];
}


void Config::load_dashboard_config(json& data_, Config& config_)
{
  if(!data_.contains("dashboard"))
    return;

  const auto& dashboard = data_["dashboard"];

  if(dashboard.contains("camera_preview"))
    config_.dashboard.camera_preview = dashboard["camera_preview"];

  if(dashboard.contains("video_port"))
    config_.dashboard.video_port = dashboard["video_port"];
}
