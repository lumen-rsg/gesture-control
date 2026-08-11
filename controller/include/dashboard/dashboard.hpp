#pragma once

#include "video.hpp"
#include "dashboard/dashboard_state.hpp"

#include <cstdint>
#include <vector>

class Dashboard
{
  public:

    Dashboard();
    ~Dashboard();

    bool initialize();

    void update(
        const inference::Result& result,
        const inference::Metrics& metrics
        );

    void update_video(
        const std::vector<uint8_t>& jpeg,
        uint64_t frame_id
        );

    void render();

    bool should_close() const;

    void shutdown();

    void set_video_server(VideoServer* server);

  private:

    struct Impl;
    Impl* impl_;

    static constexpr int WINDOW_WIDTH = 300;
    static constexpr int WINDOW_HEIGHT = 660;

    DashboardState state_;

    void update_camera_texture();
};
