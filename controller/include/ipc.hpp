#pragma once

#include <string>

#include "protocol/gesture.hpp"


class IPCServer
{
public:

    explicit IPCServer(const std::string& socket_path);

    ~IPCServer();

    bool receive(gesture::Frame& frame);

private:

    std::string socket_path_;

    int server_fd_;
    int client_fd_;

    bool setup();

    bool deserialize(
        const char* buffer,
        gesture::Frame& frame
    );
};
