#pragma once

#include "dashboard/dashboard_state.hpp"


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

    void render();

    bool should_close() const;

    void shutdown();

  private:

    struct Impl;
    Impl* impl_;

    DashboardState state_;
};
