#include "dashboard/dashboard.hpp"

#include <cstdio>
#include <vector>

#include <GLFW/glfw3.h>
#include <GL/gl.h>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

#include "video.hpp"


struct Dashboard::Impl
{
  GLFWwindow* window = nullptr;

  //
  // Camera texture
  //
  GLuint camera_texture = 0;

  int camera_width = 0;
  int camera_height = 0;

  uint64_t camera_frame_id = 0;

  //
  // Last frame received from VideoServer.
  //
  std::vector<uint8_t> video_frame;

  //
  // Temporary decoded frame.
  //
  cv::Mat decoded_frame;

  //
  // Video server.
  //
  VideoServer* video_server = nullptr;
};


Dashboard::Dashboard() :
  impl_(new Impl())
{
}


Dashboard::~Dashboard()
{
  shutdown();

  delete impl_;
}


bool Dashboard::initialize()
{
  if(!glfwInit()) {
    std::fprintf( stderr, "Failed to initialize GLFW\n");
    return false;
  }


  //
  // OpenGL 3.3 Core
  //

  glfwWindowHint( GLFW_CONTEXT_VERSION_MAJOR, 3);

  glfwWindowHint( GLFW_CONTEXT_VERSION_MINOR, 3);

  glfwWindowHint( GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


  impl_->window = glfwCreateWindow(
      WINDOW_WIDTH,
      WINDOW_HEIGHT,
      "Neuromorphic AI Demonstrator",
      nullptr,
      nullptr
      );

  if(!impl_->window) {

    std::fprintf( stderr, "Failed to create GLFW window\n");

    glfwTerminate();

    return false;
  }


  glfwMakeContextCurrent( impl_->window);

  glfwSwapInterval(1);


  //
  // Dear ImGui
  //

  IMGUI_CHECKVERSION();

  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();

  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;


  ImGui::StyleColorsDark();


  if(!ImGui_ImplGlfw_InitForOpenGL( impl_->window, true)) {

    std::fprintf( stderr, "Failed to initialize ImGui GLFW backend\n");

    return false;
  }


  if(!ImGui_ImplOpenGL3_Init( "#version 330")) {

    std::fprintf( stderr, "Failed to initialize ImGui OpenGL backend\n");

    return false;
  }


  //
  // Camera texture
  //

  glGenTextures( 1, &impl_->camera_texture);

  glBindTexture( GL_TEXTURE_2D, impl_->camera_texture);

  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

  glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindTexture( GL_TEXTURE_2D, 0);


  return true;
}


void Dashboard::set_video_server( VideoServer* server)
{
  impl_->video_server = server;
}


void Dashboard::update_camera_texture()
{
  if(!impl_->video_server)
    return;


  std::vector<uint8_t> frame;
  uint64_t frame_id = 0;

  if(!impl_->video_server->get_frame(frame, frame_id))
    return;


  if(frame_id == impl_->camera_frame_id)
    return;

  impl_->camera_frame_id = frame_id;


  //
  // Decode JPEG.
  //

  cv::Mat encoded( 1, static_cast<int>(frame.size()), CV_8UC1, frame.data());

  cv::Mat decoded = cv::imdecode( encoded, cv::IMREAD_COLOR);


  if(decoded.empty())
    return;


  //
  // Convert BGR -> RGB.
  //

  cv::cvtColor( decoded, decoded, cv::COLOR_BGR2RGB);

  //
  // Upload frame to OpenGL.
  //

  glBindTexture( GL_TEXTURE_2D, impl_->camera_texture);


  glTexImage2D(
      GL_TEXTURE_2D,
      0,
      GL_RGB,
      decoded.cols,
      decoded.rows,
      0,
      GL_RGB,
      GL_UNSIGNED_BYTE,
      decoded.data
      );


  glBindTexture( GL_TEXTURE_2D, 0);


  impl_->camera_width = decoded.cols;
  impl_->camera_height = decoded.rows;
}


void Dashboard::update(
    const inference::Result& result,
    const inference::Metrics& metrics
    )
{
  state_.result = result;
  state_.metrics = metrics;

  state_.connected = true;


  //
  // Keep latency history bounded.
  //

  constexpr std::size_t MAX_HISTORY = 120;

  state_.latency_history.push_back(
      static_cast<float>(
        metrics.total_latency_us
        ) / 1000.0f
      );


  if(state_.latency_history.size() > MAX_HISTORY) {
    state_.latency_history.erase( state_.latency_history.begin());
  }


  //
  // FPS history
  //

  state_.fps_history.push_back( metrics.fps);


  if(state_.fps_history.size() > MAX_HISTORY) {
    state_.fps_history.erase( state_.fps_history.begin());
  }
}


void Dashboard::render()
{
  if(!impl_->window)
    return;


  glfwPollEvents();


  //
  // Update camera texture before starting ImGui frame.
  //

  update_camera_texture();


  ImGui_ImplOpenGL3_NewFrame();

  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();


  //
  // Main window
  //

  ImGui::SetNextWindowPos( ImVec2(0, 0), ImGuiCond_Always);

  int width;
  int height;

  glfwGetFramebufferSize(
      impl_->window,
      &width,
      &height
      );


  ImGui::SetNextWindowSize(
      ImVec2(
        static_cast<float>(width),
        static_cast<float>(height)
        ),
      ImGuiCond_Always
      );


  ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoSavedSettings;

  ImGui::Begin( "Dashboard", nullptr, flags);

  //
  // Header
  //

  ImGui::Text( "NEUROMORPHIC AI DEMONSTRATOR");

  ImGui::Separator();

  ImGui::Spacing();


  //
  // Camera
  //

  ImGui::Text("CAMERA");

  ImGui::Spacing();


  if(impl_->camera_texture != 0 &&
      impl_->camera_width > 0 &&
      impl_->camera_height > 0) {

    float available_width = ImGui::GetContentRegionAvail().x;

    float aspect_ratio =
      static_cast<float>(impl_->camera_width) /
      static_cast<float>(impl_->camera_height);

    float image_width = available_width;

    float image_height = image_width / aspect_ratio;


    ImGui::Image(
        (ImTextureID)(intptr_t)impl_->camera_texture,
        ImVec2( image_width, image_height)
        );
  } else {
    ImGui::TextDisabled( "Waiting for camera...");
  }


  ImGui::Spacing();

  ImGui::Separator();

  ImGui::Spacing();


  //
  // Neural network
  //

  ImGui::Text( "NEURAL NETWORK");

  ImGui::Spacing();

  switch(state_.result.classification.class_id)
  {
    case 1:
      ImGui::Text("FIST");
      break;

    case 2:
      ImGui::Text("OPEN PALM");
      break;

    case 3:
      ImGui::Text("OK");
      break;

    case 4:
      ImGui::Text("ONE");
      break;

    default:
      ImGui::Text("NO CLASSIFICATION");
      break;
  }

  ImGui::Text( "Confidence: %.1f%%", state_.result.classification.confidence * 100.0f);

  ImGui::Spacing();

  ImGui::Separator();

  ImGui::Spacing();


  //
  // Performance
  //

  ImGui::Text( "PERFORMANCE");

  ImGui::Spacing();

  ImGui::Text( "FPS: %.1f", state_.metrics.fps);

  ImGui::Text( "Inference: %.3f ms", state_.metrics.inference_time_us / 1000.0f);

  ImGui::Text( "Transfer: %.3f ms", state_.metrics.transfer_time_us / 1000.0f);

  ImGui::Text( "Total latency: %.3f ms", state_.metrics.total_latency_us / 1000.0f);

  ImGui::Spacing();


  if(!state_.latency_history.empty()) {

    ImGui::PlotLines(
        " ",
        state_.latency_history.data(),
        static_cast<int>(
          state_.latency_history.size()
          ),
        0,
        nullptr,
        0.0f,
        100.0f,
        ImVec2(-1, 100)
        );
  }


  ImGui::Spacing();

  ImGui::Separator();

  ImGui::Spacing();


  //
  // Device
  //

  ImGui::Text( "DEVICE");

  ImGui::Spacing();

  ImGui::Text( "CPU: %.1f%%", state_.metrics.cpu_usage);

  ImGui::Text( "Memory: %.1f%%", state_.metrics.memory_usage);

  ImGui::Text( "Temperature: %.1f C", state_.metrics.temperature);

  ImGui::Spacing();

  if(state_.connected) {
    ImGui::Text( "Connection: CONNECTED");
  } else {
    ImGui::Text( "Connection: DISCONNECTED");
  }

  ImGui::End();


  //
  // Render
  //

  ImGui::Render();

  glViewport( 0, 0, width, height);

  glClearColor( 0.05f, 0.05f, 0.05f, 1.0f);

  glClear( GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData( ImGui::GetDrawData());


  glfwSwapBuffers( impl_->window);
}


bool Dashboard::should_close() const
{
  if(!impl_->window)
    return true;

  return glfwWindowShouldClose( impl_->window);
}


void Dashboard::shutdown()
{
  if(!impl_)
    return;

  if(impl_->camera_texture != 0) {

    glDeleteTextures( 1, &impl_->camera_texture);

    impl_->camera_texture = 0;
  }

  if(!impl_->window)
    return;

  ImGui_ImplOpenGL3_Shutdown();

  ImGui_ImplGlfw_Shutdown();

  ImGui::DestroyContext();


  glfwDestroyWindow( impl_->window);

  impl_->window = nullptr;

  glfwTerminate();
}
