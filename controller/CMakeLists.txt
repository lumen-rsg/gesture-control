cmake_minimum_required(VERSION 3.16)

project(gesture-controller
  LANGUAGES CXX
)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


add_executable(controller
  main.cpp

  src/ipc.cpp
  src/input.cpp
  src/config.cpp
  src/recognizer.cpp
)


target_include_directories(controller
  PRIVATE
  include
)
