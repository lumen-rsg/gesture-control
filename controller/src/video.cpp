#include "video.hpp"

#include <cstring>
#include <iostream>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>


namespace
{
  constexpr std::size_t HEADER_SIZE = 4;
}

VideoServer::VideoServer(uint16_t port) :
  port_(port),
  server_fd_(-1),
  client_fd_(-1),
  running_(false)
{
}

VideoServer::~VideoServer()
{
  stop();
}

bool VideoServer::start()
{
  if(running_)
    return true;

  if(!setup())
    return false;

  running_ = true;

  thread_ = std::thread( &VideoServer::run, this);

  return true;
}

void VideoServer::stop()
{
  if(!running_)
    return;

  running_ = false;

  /*
   * Закрываем сокеты, чтобы разблокировать recv()/accept().
   */
  if(client_fd_ >= 0) {
    shutdown(client_fd_, SHUT_RDWR);
    close(client_fd_);
    client_fd_ = -1;
  }

  if(server_fd_ >= 0) {
    shutdown(server_fd_, SHUT_RDWR);
    close(server_fd_);
    server_fd_ = -1;
  }

  if(thread_.joinable())
    thread_.join();
}

bool VideoServer::setup()
{
  server_fd_ = socket( AF_INET, SOCK_STREAM, 0);

  if(server_fd_ < 0) {
    perror("video socket");
    return false;
  }

  int reuse = 1;

  setsockopt( server_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

  sockaddr_in addr{};

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = INADDR_ANY;

  if(bind( server_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    perror("video bind");

    close(server_fd_);
    server_fd_ = -1;

    return false;
  }

  if(listen(server_fd_, 1) < 0) {
    perror("video listen");

    close(server_fd_);
    server_fd_ = -1;

    return false;
  }

  std::cout << "Video server FD: " << server_fd_ << "\n";

  std::cout << "Video server listening on port " << port_ << "\n";

  return true;
}

void VideoServer::run()
{
  while(running_) {
    std::cout << "Waiting for video client...\n";

    client_fd_ = accept( server_fd_, nullptr, nullptr);

    if(client_fd_ < 0) {

      if(running_)
        perror("video accept");

      break;
    }

    std::cout << "Video client connected\n";

    while(running_) {
      if(!receive_frame()) {
        if(running_)
          std::cout << "Video client disconnected\n";
        break;
      }
    }


    if(client_fd_ >= 0) {
      close(client_fd_);
      client_fd_ = -1;
    }
  }
}


bool VideoServer::receive_frame()
{
  uint32_t size = 0;

  if(!receive_exact( &size, sizeof(size))) {
    return false;
  }

  size = ntohl(size);

  if(size == 0 || size > MAX_FRAME_SIZE) {

    std::cerr << "Invalid video frame size: " << size << "\n";

    return false;
  }

  std::vector<uint8_t> frame(size);

  if(!receive_exact( frame.data(), frame.size())) {
    return false;
  }

  {
    std::lock_guard<std::mutex> lock(mutex_);

    latest_frame_ = std::move(frame);
    ++latest_frame_id_;
  }

  return true;
}


bool VideoServer::receive_exact( void* buffer, std::size_t size)
{
  std::size_t received = 0;

  while(received < size) {
    ssize_t count = recv( client_fd_, static_cast<char*>(buffer) + received, size - received, 0);

    if(count <= 0)
      return false;

    received += count;
  }

  return true;
}

bool VideoServer::get_frame( std::vector<uint8_t>& frame, uint64_t& frame_id)
{
  std::lock_guard<std::mutex> lock(mutex_);

  if(latest_frame_.empty())
    return false;

  frame = latest_frame_;
  frame_id = latest_frame_id_;

  return true;
}
