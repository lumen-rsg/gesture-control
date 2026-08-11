#include "dashboard/dashboard.hpp"

#include <cstdio>

#include <GLFW/glfw3.h>

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"


struct Dashboard::Impl
{
  GLFWwindow* window = nullptr;
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
    std::fprintf(stderr, "Failed to initialize GLFW\n");

    return false;
  }


  //
  // OpenGL 3.3 Core
  //

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);

  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);


  impl_->window = glfwCreateWindow(
      1200,
      800,
      "Neuromorphic AI Demonstrator",
      nullptr,
      nullptr
      );

  if(!impl_->window) {

    std::fprintf(stderr, "Failed to create GLFW window\n");

    glfwTerminate();

    return false;
  }


  glfwMakeContextCurrent(impl_->window);
  glfwSwapInterval(1);


  //
  // Dear ImGui
  //

  IMGUI_CHECKVERSION();

  ImGui::CreateContext();

  ImGuiIO& io = ImGui::GetIO();

  io.ConfigFlags |=
    ImGuiConfigFlags_NavEnableKeyboard;


  ImGui::StyleColorsDark();


  if(!ImGui_ImplGlfw_InitForOpenGL(impl_->window, true)) {
    std::fprintf(stderr, "Failed to initialize ImGui GLFW backend\n");

    return false;
  }


  if(!ImGui_ImplOpenGL3_Init( "#version 330")) {
    std::fprintf(stderr, "Failed to initialize ImGui OpenGL backend\n");

    return false;
  }


  return true;
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

  state_.latency_history.push_back(static_cast<float>( metrics.total_latency_us) / 1000.0f);


  if(state_.latency_history.size() > MAX_HISTORY) {

    state_.latency_history.erase(state_.latency_history.begin());
  }


  //
  // FPS history
  //

  state_.fps_history.push_back(metrics.fps);


  if(state_.fps_history.size() > MAX_HISTORY) {

    state_.fps_history.erase(state_.fps_history.begin());
  }
}


void Dashboard::render()
{
  if(!impl_->window)
    return;


  glfwPollEvents();


  ImGui_ImplOpenGL3_NewFrame();
  ImGui_ImplGlfw_NewFrame();

  ImGui::NewFrame();


  //
  // Main window
  //

  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);

  int width;
  int height;

  glfwGetFramebufferSize(impl_->window, &width, &height);


  ImGui::SetNextWindowSize(
        ImVec2( static_cast<float>(width), static_cast<float>(height)),
        ImGuiCond_Always
      );


  ImGuiWindowFlags flags =
    ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoSavedSettings;


  ImGui::Begin("Dashboard", nullptr, flags);


  //
  // Header
  //

  ImGui::Text("NEUROMORPHIC AI DEMONSTRATOR");
  ImGui::Separator();

  ImGui::Spacing();


  //
  // Neural network
  //

  ImGui::Text("NEURAL NETWORK");

  ImGui::Spacing();


  if( state_.result.classification.class_id == 0) {
    ImGui::Text("FIST");
  }
  else if(state_.result.classification.class_id == 1) {
    ImGui::Text("OPEN HAND");
  } else {
    ImGui::Text("NO CLASSIFICATION");
  }


  ImGui::Text("Confidence: %.1f%%", state_.result.classification.confidence * 100.0f);


  ImGui::Spacing();

  ImGui::Separator();

  ImGui::Spacing();


  //
  // Performance
  //

  ImGui::Text("PERFORMANCE");
  
  ImGui::Spacing();


  ImGui::Text("FPS: %.1f", state_.metrics.fps);

  ImGui::Text("Inference: %.3f ms", state_.metrics.inference_time_us / 1000.0f);

  ImGui::Text("Transfer: %.3f ms", state_.metrics.transfer_time_us / 1000.0f);

  ImGui::Text("Total latency: %.3f ms", state_.metrics.total_latency_us / 1000.0f);

  ImGui::Spacing();

  if(!state_.latency_history.empty()) {

    ImGui::PlotLines(
        "Latency",
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

  ImGui::Text("DEVICE");

  ImGui::Spacing();

  ImGui::Text("CPU: %.1f%%", state_.metrics.cpu_usage);

  ImGui::Text("Memory: %.1f%%", state_.metrics.memory_usage);

  ImGui::Text("Temperature: %.1f C", state_.metrics.temperature);

  ImGui::Spacing();

  if(state_.connected) {
    ImGui::Text("Connection: CONNECTED");
  } else {
    ImGui::Text("Connection: DISCONNECTED");
  }


  ImGui::End();


  //
  // Render
  //

  ImGui::Render();


  glViewport(0, 0, width, height);

  glClearColor(0.05f, 0.05f, 0.05f, 1.0f);

  glClear(GL_COLOR_BUFFER_BIT);

  ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

  glfwSwapBuffers( impl_->window); 
}


bool Dashboard::should_close() const
{
  if(!impl_->window)
    return true;

  return glfwWindowShouldClose(impl_->window);
}


void Dashboard::shutdown()
{
  if(!impl_ || !impl_->window)
    return;


  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();

  ImGui::DestroyContext();

  glfwDestroyWindow(impl_->window);

  impl_->window = nullptr;

  glfwTerminate();
}
