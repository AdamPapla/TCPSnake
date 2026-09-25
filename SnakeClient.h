#pragma once
#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <format>
#include <functional>
#include <iostream>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <optional>
#include <queue>
#include <random>
#include <span>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>

#include "BytesOperator.h"
#include "Messages.h"
#include "Serializers.h"
#include "SnakeCommon.h"
#include "TSQueue.h"

namespace Network {

class SnakeClient {
 public:
   SnakeClient( TSQueue< Message::ServerMessage > & ingressQueue,
                TSQueue< Message::ClientMessage > & egressQueue,
                std::stop_token stop )
       : ingressQueue_{ ingressQueue }, egressQueue_{ egressQueue }, stop_{ stop } {}

   bool connect( std::string_view ipAddr, std::uint16_t port );
   void doNetworkLoop();
   void interrupt();
   ~SnakeClient() { close( sock_ ); }

 private:
   void recvLoop();
   void sendLoop();
   void dispatchOutgoing();

   constinit static const std::size_t buffSize_{ 8192 };

   TSQueue< Message::ServerMessage > & ingressQueue_;
   TSQueue< Message::ClientMessage > & egressQueue_;

   std::array< std::uint8_t, buffSize_ > ingressBuff_;
   SessionCommon::Accumulator< std::uint8_t > accumulator_;

   std::array< std::uint8_t, buffSize_ > egressBuff_;
   std::size_t writeOffset_{ 0 };
   std::size_t sendOffset_{ 0 };

   std::condition_variable cv_;
   std::stop_token stop_;
   int sock_;
};

} // namespace Network
