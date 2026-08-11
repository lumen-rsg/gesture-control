#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "protocol/inference.hpp"


class IPCServer
{
  public:

    enum class MessageType
    {
      RESULT,
      METRICS
    };


    explicit IPCServer(uint16_t port);

    ~IPCServer();


    bool receive(
        MessageType& type,
        inference::Result& result,
        inference::Metrics& metrics
        );


  private:

    uint16_t port_;

    int server_fd_;
    int client_fd_;

    bool setup();

    bool receive_message(
        uint8_t& message_type,
        std::vector<char>& payload
        );

    bool deserialize_result(
        const char* buffer,
        std::size_t size,
        inference::Result& result
        );

    bool deserialize_metrics(
        const char* buffer,
        std::size_t size,
        inference::Metrics& metrics
        );
};
