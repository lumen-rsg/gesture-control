#include "config.hpp"

#include <fstream>
#include <iostream>

#include "json.hpp"


using json = nlohmann::json;

Config Config::load(const std::string& path)
{
  Config config;

  std::ifstream file(path);

  if(!file) {
    std::cerr
      << "Cannot open config: "
      << path
      << "\n";

    return config;
  }

  json data;

  file >> data;

  auto input = data["input"];

  if(input.contains("invert_x"))
    config.input.invert_x = input["invert_x"];

  if(input.contains("invert_y"))
    config.input.invert_y = input["invert_y"];

  if(input.contains("screen_width"))
    config.input.screen_width = input["screen_width"];

  if(input.contains("screen_height"))
    config.input.screen_height = input["screen_height"];

  return config;
}
