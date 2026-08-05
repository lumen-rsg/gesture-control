#pragma once

#include <string>


struct InputConfig
{
  bool invert_x = true;
  bool invert_y = false;

  int screen_width = 32767;
  int screen_height = 32767;
};


class Config
{
public:

    static Config load(const std::string& path);

    InputConfig input;
};
