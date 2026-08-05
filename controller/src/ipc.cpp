#include "ipc.hpp"

#include <cstring>
#include <iostream>

#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>


IPCServer::IPCServer(const std::string& socket_path) :
  socket_path_(socket_path),
  server_fd_(-1),
  client_fd_(-1)
{
  setup();
}

IPCServer::~IPCServer()
{
  if(client_fd_ >= 0)
    close(client_fd_);

  if(server_fd_ >= 0)
    close(server_fd_);

  unlink(socket_path_.c_str());
}

bool IPCServer::setup()
{
    server_fd_ = socket(
        AF_UNIX,
        SOCK_STREAM,
        0
    );

    if (server_fd_ < 0) {
        perror("socket");
        return false;
    }

    sockaddr_un addr{};

    addr.sun_family = AF_UNIX;

    strncpy(
        addr.sun_path,
        socket_path_.c_str(),
        sizeof(addr.sun_path) - 1
    );

    unlink(socket_path_.c_str());

    if (bind(
        server_fd_,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
    ) < 0)
    {
        perror("bind");
        return false;
    }

    if (listen(server_fd_, 1) < 0) {
        perror("listen");
        return false;
    }

    std::cout << "Waiting for vision module...\n";

    client_fd_ = accept(
        server_fd_,
        nullptr,
        nullptr
    );

    if (client_fd_ < 0) {
        perror("accept");
        return false;
    }

    std::cout << "Vision connected\n";

    return true;
}

bool IPCServer::receive(gesture::Frame& frame)
{
  constexpr size_t PACKET_SIZE =
    sizeof(uint32_t) +                       // version
    sizeof(uint64_t) +                       // timestamp
    sizeof(uint8_t) +                        // hand_count
    gesture::MAX_HANDS *
    gesture::LANDMARK_COUNT *
    gesture::LANDMARK_FLOATS *
    sizeof(float);

  char buffer[PACKET_SIZE];

  size_t received = 0;

  while(received < PACKET_SIZE)
  {
    ssize_t result = recv(
        client_fd_,
        buffer + received,
        PACKET_SIZE - received,
        0
        );

    if(result <= 0)
      return false;

    received += result;
  }

  return deserialize(
      buffer,
      frame
      );
}


bool IPCServer::deserialize(
    const char* buffer,
    gesture::Frame& frame
    )
{
  size_t offset = 0;


  uint32_t version;

  memcpy(
      &version,
      buffer + offset,
      sizeof(version)
      );

  offset += sizeof(version);

  if(version != gesture::PROTOCOL_VERSION)
  {
    std::cerr
      << "Protocol version mismatch\n";

    return false;
  }

  memcpy(
      &frame.timestamp,
      buffer + offset,
      sizeof(frame.timestamp)
      );

  offset += sizeof(frame.timestamp);

  memcpy(
      &frame.hand_count,
      buffer + offset,
      sizeof(frame.hand_count)
      );

  offset += sizeof(frame.hand_count);

  if(frame.hand_count > gesture::MAX_HANDS)
  {
    std::cerr << "Invalid hand count\n";

    return false;
  }

  for(size_t h = 0; h < gesture::MAX_HANDS; h++)
  {
    for(size_t l = 0; l < gesture::LANDMARK_COUNT; l++)
    {
      auto& landmark = frame.hands[h].landmarks[l];

      memcpy(
          &landmark.x,
          buffer + offset,
          sizeof(float)
          );

      offset += sizeof(float);

      memcpy(
          &landmark.y,
          buffer + offset,
          sizeof(float)
          );

      offset += sizeof(float);

      memcpy(
          &landmark.z,
          buffer + offset,
          sizeof(float)
          );

      offset += sizeof(float);
    }
  }

  return true;
}
