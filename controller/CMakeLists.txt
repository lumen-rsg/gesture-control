cmake_minimum_required(VERSION 3.16)

project(gesture-controller
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


#
# Dependencies
#

find_package(OpenGL REQUIRED)
find_package(glfw3 REQUIRED)


#
# Dear ImGui
#

set(IMGUI_DIR
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui
)


add_library(imgui STATIC

  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp

  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
)


target_include_directories(imgui
  PUBLIC

  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)


target_link_libraries(imgui
  PUBLIC

  glfw
  OpenGL::GL
)


#
# Controller
#

add_executable(controller

  main.cpp

  src/ipc.cpp
  src/input.cpp
  src/config.cpp
  src/recognizer.cpp

  src/dashboard/dashboard.cpp
  src/dashboard/dashboard_state.cpp
)


target_include_directories(controller
  PRIVATE

  include
)


target_link_libraries(controller
  PRIVATE

  imgui
)
