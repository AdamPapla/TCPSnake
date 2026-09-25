#include "SnakeClient.h"
#include "Messages.h"
#include "SnakeCommon.h"
#include "SnakeSerdes.h"
#include <sys/socket.h>

namespace Network {

bool
SnakeClient::connect( std::string_view serverAddr, std::uint16_t port ) {
   sock_ = socket( AF_INET, SOCK_STREAM, 0 );
   LOG( "Connecting to server on addr {}, port {}", serverAddr, port );

   sockaddr_in server{};
   server.sin_family = AF_INET;
   inet_pton( AF_INET, serverAddr.data(), &server.sin_addr );
   server.sin_port = htons( port );
   return ::connect( sock_,
                     reinterpret_cast< sockaddr * >( &server ),
                     sizeof( server ) ) == 0;
}

void
SnakeClient::doNetworkLoop() {
   std::jthread recvThread{ [ & ]() { this->recvLoop(); } };
   std::jthread sendThread{ [ & ]() { this->sendLoop(); } };
}

void
SnakeClient::recvLoop() {
   LOG( "Starting receive loop on client listening to socket {}", sock_ );
   while ( !stop_.stop_requested() ) {
      auto readBytes = ::recv( sock_, ingressBuff_.data(), ingressBuff_.size(), 0 );
      // TODO: Add proper error handling here
      assert( readBytes != -1 && "recv call failed" );
      LOG( "Received {} bytes on client", readBytes );
      if ( readBytes > 0 ) {
         accumulator_.insert( accumulator_.end(),
                              ingressBuff_.begin(),
                              ingressBuff_.begin() + readBytes );
         SessionCommon::onReceive< Message::ServerMessage >(
             accumulator_, ingressQueue_, stop_ );
      }
   }
}

void
SnakeClient::sendLoop() {
   while ( !stop_.stop_requested() ) {
      dispatchOutgoing();
   }
}

void
SnakeClient::dispatchOutgoing() {
   auto toSend = SessionCommon::prepareOutgoing(
       egressBuff_, writeOffset_, sendOffset_, egressQueue_, stop_ );
   if ( toSend.empty() ) {
      return;
   }
   auto sentBytes = ::send( sock_, toSend.data(), toSend.size(), 0 );
   LOG( "Sent {} bytes from client", sentBytes );
   assert( sentBytes != -1 && "send call failed" );
   sendOffset_ += sentBytes;
   // TODO: Again - ring buffers make this clean. This is just a stop-gap
   if ( sendOffset_ == writeOffset_ ) {
      sendOffset_ = 0;
      writeOffset_ = 0;
   }
}

void
SnakeClient::interrupt() {
   shutdown( sock_, SHUT_RDWR );
}

} // namespace Network
