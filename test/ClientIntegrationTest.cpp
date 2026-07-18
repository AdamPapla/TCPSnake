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
                    std::promise< std::uint16_t > portPromise )
       : messages_{ std::move( messages ) },
         portPromise_{ std::move( portPromise ) } {
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
                Serdes::transfer( msg, counter );
                writer.transfer( counter.count );
                Serdes::transfer( msg, writer );
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

struct TestCase {
   std::string name;
   std::vector< Message::ServerMessage > messages;
};

class ClientIntegrationTest : public ::testing::TestWithParam< TestCase > {
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

   bool startClient( const std::string_view ipAddr, uint16_t port ) {
      std::promise< bool > connectedProm;
      std::future< bool > connectedFut = connectedProm.get_future();
      clientThread_ = std::jthread{ [ &, p = std::move( connectedProm ) ]() mutable {
         bool connected = snakeClient()->connect( ipAddr.data(), port );
         p.set_value( connected );
         if ( connected )
            snakeClient()->doNetworkLoop();
      } };
      return connectedFut.get();
   }

   void stopClient() {
      snakeClient()->interrupt();
      stop_.request_stop();
   }

   Network::SnakeClient *snakeClient() { return snakeClient_.get(); }

   bool waitFor( std::function< bool() > pred,
                 std::chrono::milliseconds timeout ) const {
      auto deadline = std::chrono::steady_clock::now() + timeout;
      while ( std::chrono::steady_clock::now() < deadline ) {
         if ( pred() ) {
            return true;
         }
         std::this_thread::sleep_for( std::chrono::milliseconds( 10 ) );
      }
      return false;
   }

 protected:
   TSQueue< Message::ClientMessage > egress_;
   TSQueue< Message::ServerMessage > ingress_;
   std::stop_source stop_;
   std::jthread clientThread_;
   std::unique_ptr< Network::SnakeClient > snakeClient_;
   std::unique_ptr< FakeSnakeServer > snakeServer_;
};

// TODO: This should really just be a parameterized test with different messages
TEST_P( ClientIntegrationTest, BasicIngressTest ) {
   auto messages = GetParam().messages;

   std::promise< std::uint16_t > portProm;
   auto portFut = portProm.get_future();
   startServer( messages, std::move( portProm ) );
   std::uint16_t port = portFut.get();

   ASSERT_TRUE( startClient( "127.0.0.1", port ) );
   ASSERT_TRUE( waitFor( [ & ]() { return ingress_.size() == messages.size(); },
                         std::chrono::seconds( 5 ) ) );

   EXPECT_EQ( ingress_.size(), messages.size() );
   for ( auto & msg : messages ) {
      ASSERT_EQ( ingress_.pop(), msg );
   }
}

static const std::vector< TestCase > messages{
    { "SingleRegisterAck",
      { Message::AckMessage{ { Message::ServerMessageType::REGISTER_ACK },
                             1,
                             Message::NackReason::UNSET } } },
    { "SingleJoinAck",
      { Message::AckMessage{ { Message::ServerMessageType::JOIN_ACK },
                             1,
                             Message::NackReason::UNSET } } },
    { "SingleChangeDirAck",
      { Message::AckMessage{ { Message::ServerMessageType::CHANGE_DIR_ACK },
                             1,
                             Message::NackReason::UNSET } } },
    { "SingleLeaveAck",
      { Message::AckMessage{ { Message::ServerMessageType::LEAVE_ACK },
                             1,
                             Message::NackReason::UNSET } } },
    { "SingleDisconnect",
      { Message::DisconnectMessage{ { Message::ServerMessageType::DISCONNECT },
                                    "reason" } } },
    { "SingleDeath",
      { Message::DeathMessage{ { Message::ServerMessageType::DEATH }, 42 } } },
    { "SingleSnapshot",
      { Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT },
                                  { 0x01, 0x02, 0x03, 0x04 } } } },

    // --- Edge cases ---
    { "EmptyDisconnectReason",
      { Message::DisconnectMessage{ { Message::ServerMessageType::DISCONNECT },
                                    "" } } },
    { "ZeroScore",
      { Message::DeathMessage{ { Message::ServerMessageType::DEATH }, 0 } } },
    { "MaxScore",
      { Message::DeathMessage{ { Message::ServerMessageType::DEATH },
                               std::numeric_limits< uint32_t >::max() } } },
    { "EmptySnapshot",
      { Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT }, {} } } },

    // --- Multi-message tests ---
    { "RegisterThenJoinAck",
      { Message::AckMessage{ { Message::ServerMessageType::REGISTER_ACK },
                             1,
                             Message::NackReason::UNSET },
        Message::AckMessage{ { Message::ServerMessageType::JOIN_ACK },
                             1,
                             Message::NackReason::UNSET } } },
    { "FullSessionFlow",
      { Message::AckMessage{ { Message::ServerMessageType::REGISTER_ACK },
                             1,
                             Message::NackReason::UNSET },
        Message::AckMessage{ { Message::ServerMessageType::JOIN_ACK },
                             1,
                             Message::NackReason::UNSET },
        Message::AckMessage{ { Message::ServerMessageType::CHANGE_DIR_ACK },
                             1,
                             Message::NackReason::UNSET },
        Message::DeathMessage{ { Message::ServerMessageType::DEATH }, 100 },
        Message::AckMessage{ { Message::ServerMessageType::LEAVE_ACK },
                             1,
                             Message::NackReason::UNSET } } },
    { "SnapshotBurst",
      { Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT },
                                  { 0xAA, 0xBB } },
        Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT },
                                  { 0xCC, 0xDD } },
        Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT },
                                  { 0xEE, 0xFF } } } },
    { "AckThenDisconnect",
      { Message::AckMessage{ { Message::ServerMessageType::JOIN_ACK },
                             1,
                             Message::NackReason::UNSET },
        Message::DisconnectMessage{ { Message::ServerMessageType::DISCONNECT },
                                    "idle" } } },
};

INSTANTIATE_TEST_SUITE_P( BasicClientPipelineTest,
                          ClientIntegrationTest,
                          ::testing::ValuesIn( messages ),
                          []( const testing::TestParamInfo< TestCase > & test ) {
                             return test.param.name;
                          } );
