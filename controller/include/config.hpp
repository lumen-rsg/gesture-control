#pragma once

#include <cstdint>
#include <string>

#include "json.hpp"

using json = nlohmann::json;

struct InputConfig
{
  bool invert_x = true;
  bool invert_y = false;

  int screen_width = 32767;
  int screen_height = 32767;
};

struct DashboardConfig
{
  bool camera_preview = false;

  uint16_t video_port = 5001;
};

class Config
{
  public:

    InputConfig input;

    DashboardConfig dashboard;

    static Config load(const std::string& path);

  private:

    static void load_input_config(json &data_, Config &config_);

    static void load_dashboard_config(json &data_, Config &config_);
};
