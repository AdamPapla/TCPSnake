#include "SnakeClient.h"
#include "Messages.h"
#include "SnakeSerdes.h"
#include <sys/socket.h>

namespace Network {

bool
SnakeClient::connect( std::string_view serverAddr, std::uint16_t port ) {
   sock_ = socket( AF_INET, SOCK_STREAM, 0 );

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
   while ( !stop_.stop_requested() ) {
      auto readBytes = ::recv( sock_, ingressBuff_.data(), ingressBuff_.size(), 0 );
      // TODO: Add proper error handling here
      assert( readBytes != -1 && "recv call failed" );
      if ( readBytes > 0 ) {
         accumulator_.insert( accumulator_.end(),
                              ingressBuff_.begin(),
                              ingressBuff_.begin() + readBytes );
         drainIngressQueue();
         maybeCompactAccumulator();
      }
   }
}

void
SnakeClient::sendLoop() {
   while ( !stop_.stop_requested() ) {
      queueOutgoing();
      sendFromBuffer();
   }
}

void
SnakeClient::drainIngressQueue() {
   using namespace Message;
   // TODO: Good opportunity to practice circular buffers here, for now just
   // going with simple read offset + occasional resize.
   std::span< std::uint8_t > toRead{ accumulator_.begin() + readOffset_,
                                     accumulator_.end() };
   Serdes::BytesReader reader( toRead );
   while ( !stop_.stop_requested() ) {
      if ( auto msg = SnakeSerdes::readNextServerMsg( reader ); msg ) {
         assert( msg.has_value() );
         ingressQueue_.push( std::move( msg.value() ) );
      } else
         break;
   }
   readOffset_ = accumulator_.size() - reader.remainingBytes();
}

void
SnakeClient::queueOutgoing() {
   // We queue to buffer to make use of TCP's partial sends and prevent blocking
   // network thread for too long Ensure we have enough space, then write the
   // message.
   Serdes::BytesWriter writer{ std::span< uint8_t >(
       egressBuff_.begin() + writeOffset_, egressBuff_.end() ) };
   bool keepGoing = writeOffset_ < buffSize_;
   while ( !stop_.stop_requested() && keepGoing ) {
      auto outbound = egressQueue_.try_front();
      if ( !outbound.has_value() )
         break;
      keepGoing = SnakeSerdes::writeNext( writer, outbound.value() );
      egressQueue_.pop();
      writeOffset_ = buffSize_ - writer.remainingBytes();
   }
}

void
SnakeClient::sendFromBuffer() {
   auto *sendStart = &( egressBuff_[ sendOffset_ ] );
   auto sendLen = writeOffset_ - sendOffset_;
   if ( sendLen == 0 ) {
      return;
   }
   auto sentBytes = ::send( sock_, sendStart, sendLen, 0 );
   assert( sentBytes != -1 && "send call failed" );
   sendOffset_ += sentBytes;
   // TODO: Again - ring buffers make this clean. This is just a stop-gap
   if ( sendOffset_ == writeOffset_ ) {
      sendOffset_ = 0;
      writeOffset_ = 0;
   }
}

// TODO: Ring buffers will remove the need for this shrinking
void
SnakeClient::maybeCompactAccumulator() {
   if ( accumulator_.size() > buffSize_ / 2 ) {
      // TODO: This may lead to wasted allocations once the buffer fills again
      accumulator_.erase( accumulator_.begin(), accumulator_.begin() + readOffset_ );
      readOffset_ = 0;
   }
}

void
SnakeClient::interrupt() {
   shutdown( sock_, SHUT_RDWR );
}

} // namespace Network
