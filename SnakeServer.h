#pragma once
#include <algorithm>
#include <cassert>
#include <chrono>
#include <format>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <optional>
#include <queue>
#include <random>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

#include "Messages.h"
#include "SnakeCommon.h"
#include "TSQueue.h"

struct ServerTcpConfig {
   int family;
};

using Tick = std::uint32_t;

namespace Network {

// Making this a struct for now in case it needs expanded later
struct ClientConnection {
   ClientConnection( uint32_t id, int s ) : clientId{ id }, sock{ s } {}

   ClientId clientId;
   int sock;

   // TODO: Move to circular buffers
   SessionCommon::Accumulator< std::uint8_t > accumulator;
   static constexpr int sendBuffSize = 2048;
   std::array< std::uint8_t, sendBuffSize > egressBuff_;
   std::size_t writeOffset_{ 0 };
   std::size_t sendOffset_{ 0 };

   TSQueue< Message::ServerMessage > egress;
   TSQueue< Message::ClientMessage > ingress;
};

// Server is single threaded, using epoll for registering new connections and
// handling writes/reads for existing clients. 
class SnakeServer {
 public:
   SnakeServer( ServerTcpConfig config, std::stop_token st )
       : stop_{ st }, config_{ config }, epollFd_{ -1 }, listenSock_{ -1 } {
      initialize();
   }
   ~SnakeServer();
   void networkLoop();
   // This can safely be called across threads. epoll_ctl is thread safe
   void markWritable( ClientConnection *client );
   const auto & connections() const { return connections_; }

 private:
   void initialize();
   void registerListener();
   void registerConnection( const int clientSock );
   bool onReadable( ClientConnection *readyClient );
   bool onWritable( ClientConnection *readyClient );
   void dispatchOutgoing();

   std::stop_token stop_;
   ServerTcpConfig config_;
   int epollFd_;
   int listenSock_;

   ClientId nextClientId_{ 1 };
   std::unordered_map< ClientId, std::unique_ptr< ClientConnection > > connections_;
   static constexpr int recvBuffSize = 2048;
   std::array< std::uint8_t, recvBuffSize > ingressBuff_;
};
} // namespace Network
