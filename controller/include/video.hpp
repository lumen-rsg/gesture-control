#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <thread>
#include <vector>


class VideoServer
{
  public:
    explicit VideoServer(uint16_t port);
    ~VideoServer();

    bool start();
    void stop();

    bool get_frame( std::vector<uint8_t>& frame, uint64_t& frame_id);

  private:

    static constexpr std::size_t MAX_FRAME_SIZE = 10 * 1024 * 1024;

    uint16_t port_;

    int server_fd_;
    int client_fd_;

    std::atomic<bool> running_;

    std::thread thread_;

    std::mutex mutex_;

    std::vector<uint8_t> latest_frame_;
    uint64_t latest_frame_id_ = 0;

    bool setup();
    void run();

    bool receive_frame();

    bool receive_exact( void* buffer, std::size_t size);
};
