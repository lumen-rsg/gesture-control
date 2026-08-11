#include "ipc.hpp"

#include <cstring>
#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "protocol/protocol.hpp"


namespace
{

  constexpr std::size_t HEADER_SIZE = 12;

  constexpr std::size_t RESULT_SIZE = 44;

  constexpr std::size_t METRICS_SIZE = 56;

  constexpr std::size_t MAX_PAYLOAD_SIZE = 1024 * 1024;


  uint32_t read_u32(const char* data)
  {
    uint32_t value;

    std::memcpy(
        &value,
        data,
        sizeof(value)
        );

    return value;
  }


  uint64_t read_u64(const char* data)
  {
    uint64_t value;

    std::memcpy(
        &value,
        data,
        sizeof(value)
        );

    return value;
  }


  float read_float(const char* data)
  {
    float value;

    std::memcpy(
        &value,
        data,
        sizeof(value)
        );

    return value;
  }


  bool receive_exact(
      int fd,
      void* buffer,
      std::size_t size
      )
  {
    std::size_t received = 0;

    while(received < size) {

      ssize_t count = recv(
          fd,
          static_cast<char*>(buffer) + received,
          size - received,
          0
          );

      if(count == 0)
        return false;

      if(count < 0)
        return false;

      received += count;
    }

    return true;
  }

}


IPCServer::IPCServer(uint16_t port) :
  port_(port),
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
}


bool IPCServer::setup()
{
  server_fd_ = socket(
      AF_INET,
      SOCK_STREAM,
      0
      );

  if(server_fd_ < 0) {
    perror("socket");
    return false;
  }


  int reuse = 1;

  setsockopt(
      server_fd_,
      SOL_SOCKET,
      SO_REUSEADDR,
      &reuse,
      sizeof(reuse)
      );


  sockaddr_in addr{};

  addr.sin_family = AF_INET;
  addr.sin_port = htons(port_);
  addr.sin_addr.s_addr = INADDR_ANY;


  if(bind(
        server_fd_,
        reinterpret_cast<sockaddr*>(&addr),
        sizeof(addr)
        ) < 0)
  {
    perror("bind");
    return false;
  }


  if(listen(server_fd_, 1) < 0) {
    perror("listen");
    return false;
  }


  std::cout
    << "Waiting for vision module on port "
    << port_
    << "...\n";


  client_fd_ = accept(
      server_fd_,
      nullptr,
      nullptr
      );


  if(client_fd_ < 0) {
    perror("accept");
    return false;
  }


  std::cout
    << "Vision connected\n";


  return true;
}


bool IPCServer::receive(
    MessageType& type,
    inference::Result& result,
    inference::Metrics& metrics
    )
{
  uint8_t message_type;
  std::vector<char> payload;


  if(!receive_message(message_type, payload))
    return false;


  switch(message_type) {

    case static_cast<uint8_t>(inference::MessageType::RESULT): {
      if(!deserialize_result(
            payload.data(),
            payload.size(),
            result
            ))
        return false;

      type = MessageType::RESULT;

      return true;
    }


    case static_cast<uint8_t>(inference::MessageType::METRICS): {
      if(!deserialize_metrics(
            payload.data(),
            payload.size(),
            metrics
            ))
        return false;

      type = MessageType::METRICS;

      return true;
    }

    default:

    std::cerr << "Unknown message type: " << static_cast<int>(message_type) << "\n";

    return false;
  }
}


bool IPCServer::receive_message(
    uint8_t& message_type,
    std::vector<char>& payload
    )
{
  char header[HEADER_SIZE];


  if(!receive_exact(
        client_fd_,
        header,
        HEADER_SIZE
        ))
  {
    return false;
  }


  std::size_t offset = 0;


  //
  // Protocol version
  //

  uint32_t version =
    read_u32(
        header + offset
        );

  offset += sizeof(uint32_t);


  //
  // Message type
  //

  message_type =
    static_cast<uint8_t>(
        header[offset]
        );

  offset += sizeof(uint8_t);


  //
  // Reserved
  //

  offset += sizeof(uint8_t);
  offset += sizeof(uint16_t);


  //
  // Payload size
  //

  uint32_t payload_size =
    read_u32(
        header + offset
        );


  //
  // Validate protocol version
  //

  if(version != inference::PROTOCOL_VERSION)
  {
    std::cerr
      << "Protocol version mismatch: "
      << version
      << " != "
      << inference::PROTOCOL_VERSION
      << "\n";

    return false;
  }


  //
  // Validate payload size
  //

  if(payload_size > MAX_PAYLOAD_SIZE)
  {
    std::cerr
      << "Payload too large: "
      << payload_size
      << "\n";

    return false;
  }


  payload.resize(payload_size);


  if(payload_size == 0)
    return true;


  return receive_exact(
      client_fd_,
      payload.data(),
      payload_size
      );
}


bool IPCServer::deserialize_result(
    const char* buffer,
    std::size_t size,
    inference::Result& result
    )
{
  if(size != RESULT_SIZE)
  {
    std::cerr
      << "Invalid RESULT payload size: "
      << size
      << "\n";

    return false;
  }


  std::size_t offset = 0;


  //
  // Frame ID
  //

  result.frame_id =
    read_u64(
        buffer + offset
        );

  offset += sizeof(uint64_t);


  //
  // Timestamp
  //

  result.timestamp =
    read_u64(
        buffer + offset
        );

  offset += sizeof(uint64_t);


  //
  // Hand present
  //
  // uint8 + 3 bytes reserved
  //

  result.hand_present =
    static_cast<uint8_t>(
        buffer[offset]
        );

  offset += 4;


  //
  // Classification
  //

  result.classification.class_id =
    read_u32(
        buffer + offset
        );

  offset += sizeof(uint32_t);


  result.classification.confidence =
    read_float(
        buffer + offset
        );

  offset += sizeof(float);


  //
  // Bounding box
  //

  result.hand.x =
    read_float(
        buffer + offset
        );

  offset += sizeof(float);


  result.hand.y =
    read_float(
        buffer + offset
        );

  offset += sizeof(float);


  result.hand.width =
    read_float(
        buffer + offset
        );

  offset += sizeof(float);


  result.hand.height =
    read_float(
        buffer + offset
        );


  return true;
}


bool IPCServer::deserialize_metrics(
    const char* buffer,
    std::size_t size,
    inference::Metrics& metrics
    )
{
  if(size != METRICS_SIZE)
  {
    std::cerr
      << "Invalid METRICS payload size: "
      << size
      << "\n";

    return false;
  }


  std::size_t offset = 0;


  //
  // Frame ID
  //

  metrics.frame_id =
    read_u64(
        buffer + offset
        );

  offset += sizeof(uint64_t);


  //
  // Timestamp
  //

  metrics.timestamp =
    read_u64(
        buffer + offset
        );

  offset += sizeof(uint64_t);


  //
  // FPS
  //

  metrics.fps =
    read_float(
        buffer + offset
        );

  offset += sizeof(float);


  //
  // Processing stages
  //

  metrics.capture_time_us =
    read_u32(
        buffer + offset
        );

  offset += sizeof(uint32_t);


  metrics.preprocess_time_us =
    read_u32(
        buffer + offset
        );

  offset += sizeof(uint32_t);


  metrics.inference_time_us =
    read_u32(
        buffer + offset
        );

  offset += sizeof(uint32_t);


  metrics.transfer_time_us =
    read_u32(
        buffer + offset
        );

  offset += sizeof(uint32_t);


  metrics.postprocess_time_us =
    read_u32(
        buffer + offset
        );

  offset += sizeof(uint32_t);


  metrics.total_latency_us =
    read_u32(
        buffer + offset
        );

  offset += sizeof(uint32_t);


  //
  // System metrics
  //

  metrics.cpu_usage =
    read_float(
        buffer + offset
        );

  offset += sizeof(float);


  metrics.memory_usage =
    read_float(
        buffer + offset
        );

  offset += sizeof(float);


  metrics.temperature =
    read_float(
        buffer + offset
        );


  return true;
}
