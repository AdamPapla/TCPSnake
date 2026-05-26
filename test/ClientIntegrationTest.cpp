#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <stop_token>
#include <sys/socket.h>

#include "BytesOperator.h"
#include "Messages.h"
#include "SnakeClient.h"

class FakeSnakeServer {
 public:
   FakeSnakeServer( std::vector< Message::ServerMessage > messages,
                    std::promise< std::uint16_t > promise )
       : messages_{ std::move( messages ) }, portPromise_{ std::move( promise ) } {
      thread_ = std::jthread( [ this ]() { this->run(); } );
   }

   void run() {
      serverSock_ = socket( AF_INET, SOCK_STREAM, 0 );
      if ( serverSock_ < 0 ) {
         return;
      }

      sockaddr_in addr{};
      addr.sin_family = AF_INET;
      addr.sin_addr.s_addr = INADDR_ANY;
      addr.sin_port = htons( 0 );

      if ( bind( serverSock_, (sockaddr *)&addr, sizeof( addr ) ) < 0 ) {
         return;
      }

      listen( serverSock_, 1 );

      // Need to get port we were assigned and communicate it to client
      socklen_t len = sizeof( addr );
      getsockname( serverSock_, (sockaddr *)&addr, &len );
      auto port = ntohs( addr.sin_port );

      portPromise_.set_value( port );
      clientSock_ = accept( serverSock_, nullptr, nullptr );
      if ( clientSock_ < 0 ) {
         return;
      }

      std::vector< uint8_t > egressBuff( 5012 );
      Serdes::BytesWriter writer( egressBuff );

      // For now, just fire off all of the messages in one go
      for ( const auto & message : messages_ ) {
         Serdes::BytesCounter counter;
         std::visit(
             [ & ]( const auto & msg ) {
                Serdes::write( msg, counter );
                writer.write( counter.count );
                Serdes::write( msg, writer );
             },
             message );
      }

      auto toSend = (int)egressBuff.size() - (int)writer.remainingBytes();

      int sent = 0;
      int rem = toSend;

      while ( sent < toSend ) {
         int sentBytes =
             ::send( clientSock_, egressBuff.data() + sent, rem, MSG_NOSIGNAL );
         if ( sentBytes <= 0 ) {
            break;
         }
         sent += sentBytes;
         rem = toSend - sent;
      }
   }

   ~FakeSnakeServer() {
      if ( serverSock_ != -1 )
         close( serverSock_ );
      if ( clientSock_ != -1 )
         close( clientSock_ );
      if ( thread_.joinable() ) {
         thread_.join();
      }
   }

 private:
   std::jthread thread_;
   std::vector< Message::ServerMessage > messages_;
   std::promise< std::uint16_t > portPromise_;
   int serverSock_{ -1 };
   int clientSock_{ -1 };
};

class ClientIntegrationTest : public ::testing::Test {
 public:
   ClientIntegrationTest()
       : snakeClient_{ std::make_unique< Network::SnakeClient >(
             this->ingress_, this->egress_, stop_.get_token() ) } {}

   void TearDown() override { stopClient(); }

   void startServer( std::vector< Message::ServerMessage > messages,
                     std::promise< std::uint16_t > promise ) {
      snakeServer_ = std::make_unique< FakeSnakeServer >( std::move( messages ),
                                                          std::move( promise ) );
   }

   void startClient( const std::string_view ipAddr, uint16_t port ) {
      clientThread_ = std::jthread{ [ & ]() {
         EXPECT_EQ( snakeClient()->connect( ipAddr.data(), port ), 0 );
         snakeClient()->doNetworkLoop();
      } };
   }

   void stopClient() {
      snakeClient()->interrupt();
      stop_.request_stop();
   }

   Network::SnakeClient *snakeClient() { return snakeClient_.get(); }

 protected:
   TSQueue< Message::ClientMessage > egress_;
   TSQueue< Message::ServerMessage > ingress_;
   std::stop_source stop_;
   std::jthread clientThread_;
   std::unique_ptr< Network::SnakeClient > snakeClient_;
   std::unique_ptr< FakeSnakeServer > snakeServer_;
};

// TODO: This should really just be a parameterized test with different messages
TEST_F( ClientIntegrationTest, BasicIngressTest ) {
   auto ack = Message::AckMessage{
       { Message::ServerMessageType::JOIN_ACK }, 1, Message::NackReason::UNSET };

   std::vector< Message::ServerMessage > messages{ ack };

   std::promise< std::uint16_t > prom;
   auto fut = prom.get_future();

   startServer( std::move( messages ), std::move( prom ) );

   std::uint16_t port = fut.get();

   // TODO: Implement a waitFor mechanism to avoid these heuristic thread sleeps
   std::this_thread::sleep_for( std::chrono::milliseconds( 200 ) );
   startClient( "ipAddr", port );
   std::this_thread::sleep_for( std::chrono::milliseconds( 300 ) );

   EXPECT_EQ( ingress_.size(), 1 );

   auto msgOpt = ingress_.try_front();
   ASSERT_TRUE( msgOpt );

   ASSERT_TRUE( std::holds_alternative< Message::AckMessage >( msgOpt.value() ) );

   auto recvAck = std::get< Message::AckMessage >( msgOpt.value() );
   EXPECT_EQ( ack.msgType, recvAck.msgType );
   EXPECT_EQ( ack.id, recvAck.id );
   EXPECT_EQ( ack.reason, recvAck.reason );
}

// TODO: Add more test cases
