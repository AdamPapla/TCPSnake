#include <chrono>
#include <future>
#include <gtest/gtest.h>
#include <iostream>
#include <memory>
#include <stop_token>
#include <sys/socket.h>

#include "BytesOperator.h"
#include "GameState.h"
#include "Messages.h"
#include "SnakeClient.h"
#include "SnakeSerdes.h"

namespace {
using namespace GameState;

Coord
makeCoord( std::uint32_t x, std::uint32_t y ) {
   Coord c{};
   c[ 0 ] = x;
   c[ 1 ] = y;
   return c;
}

SnakeSnapshot
makeSnake( ClientId id, std::initializer_list< Coord > blocks ) {
   SnakeSnapshot s{};
   s.id = id;
   s.blocks = blocks;
   return s;
}
} // namespace

class FakeSnakeServer {
 public:
   FakeSnakeServer( std::vector< Message::ServerMessage > toClient,
                    std::vector< Message::ClientMessage > fromClient,
                    std::promise< std::uint16_t > portPromise )
       : toClient_{ std::move( toClient ) }, fromClient_{ std::move( fromClient ) },
         portPromise_{ std::move( portPromise ) } {
      thread_ = std::jthread( [ this ]() { this->run(); } );
   }

   void setupConnection() {
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
   }

   void run() {
      setupConnection();
      std::vector< uint8_t > egressBuff( 5012 );
      Serdes::BytesWriter writer( egressBuff );

      // We don't really need to care about synchronization here. The function of
      // this test is to ensure the logic for sending/receiving works as-intended,
      // not to test synchronization that doens't live in the network layer.
      for ( const auto & message : toClient_ ) {
         Serdes::BytesCounter counter;
         std::visit(
             [ & ]( const auto & msg ) {
                Serdes::transfer( msg, counter );
                Serdes::transfer( counter.count, writer );
                Serdes::transfer( msg, writer );
             },
             message );
      }

      auto toClient = (int)egressBuff.size() - (int)writer.remainingBytes();

      int sent = 0;
      int rem = toClient;

      while ( sent < toClient ) {
         int sentBytes =
             ::send( clientSock_, egressBuff.data() + sent, rem, MSG_NOSIGNAL );
         if ( sentBytes <= 0 ) {
            break;
         }
         sent += sentBytes;
         rem = toClient - sent;
      }

      std::vector< uint8_t > accumulator;
      std::array< uint8_t, 2048 > ingressBuff;
      int readOffset = 0;
      while ( recvd_ < fromClient_.size() ) {
         auto readBytes =
             ::recv( clientSock_, ingressBuff.data(), ingressBuff.size(), 0 );
         assert( readBytes != -1 && "recv call failed" );
         accumulator.insert( accumulator.end(),
                             ingressBuff.begin(),
                             ingressBuff.begin() + readBytes );

         std::span< uint8_t > toRead{ accumulator.begin() + readOffset,
                                      accumulator.end() };
         Serdes::BytesReader reader( toRead );
         while ( auto nextMsg = SnakeSerdes::readNextClientMsg( reader ) ) {
            ASSERT_LE( recvd_, fromClient_.size() );
            EXPECT_EQ( nextMsg.value(), fromClient_[ recvd_ ] );
            ++recvd_;
         }
         readOffset += accumulator.size() - reader.remainingBytes();
      }
   }

   bool allMessagesReceived() const { return recvd_ == fromClient_.size(); }

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
   std::vector< Message::ServerMessage > toClient_;
   std::vector< Message::ClientMessage > fromClient_;
   std::atomic_size_t recvd_;
   std::promise< std::uint16_t > portPromise_;
   int serverSock_{ -1 };
   int clientSock_{ -1 };
};

struct TestCase {
   std::string name;
   std::vector< Message::ServerMessage > toClient;
   std::vector< Message::ClientMessage > fromClient;
};

class ClientIntegrationTest : public ::testing::TestWithParam< TestCase > {
 public:
   ClientIntegrationTest()
       : snakeClient_{ std::make_unique< Network::SnakeClient >(
             this->ingress_, this->egress_, stop_.get_token() ) } {}

   void TearDown() override { stopClient(); }

   void startServer( std::vector< Message::ServerMessage > toClient,
                     std::vector< Message::ClientMessage > fromClient,
                     std::promise< std::uint16_t > promise ) {
      snakeServer_ = std::make_unique< FakeSnakeServer >(
          std::move( toClient ), std::move( fromClient ), std::move( promise ) );
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
   FakeSnakeServer *snakeServer() { return snakeServer_.get(); }

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
   std::promise< std::uint16_t > portProm;
   auto portFut = portProm.get_future();
   startServer( GetParam().toClient, GetParam().fromClient, std::move( portProm ) );
   std::uint16_t port = portFut.get();

   ASSERT_TRUE( startClient( "127.0.0.1", port ) );
   ASSERT_TRUE(
       waitFor( [ & ]() { return ingress_.size() == GetParam().toClient.size(); },
                std::chrono::seconds( 50 ) ) );
   EXPECT_EQ( ingress_.size(), GetParam().toClient.size() );
   for ( auto & msg : GetParam().toClient ) {
      ASSERT_EQ( ingress_.pop(), msg );
   }
   for ( const auto & msg : GetParam().fromClient ) {
      egress_.push( msg );
   }
   ASSERT_TRUE( waitFor( [ & ]() { return snakeServer()->allMessagesReceived(); },
                         std::chrono::seconds( 50 ) ) );
}

// Claude generated test cases, too tedious to do by hand

namespace {
// --- Pure ingress (server -> client) ---

static const TestCase kSingleRegisterAck{
    "SingleRegisterAck",
    { Message::AckMessage{ { Message::ServerMessageType::REGISTER_ACK },
                           1,
                           Message::NackReason::UNSET } },
    {} };

static const TestCase kSingleJoinAck{
    "SingleJoinAck",
    { Message::AckMessage{
        { Message::ServerMessageType::JOIN_ACK }, 1, Message::NackReason::UNSET } },
    {} };

static const TestCase kSingleChangeDirAck{
    "SingleChangeDirAck",
    { Message::AckMessage{ { Message::ServerMessageType::CHANGE_DIR_ACK },
                           1,
                           Message::NackReason::UNSET } },
    {} };

static const TestCase kSingleLeaveAck{
    "SingleLeaveAck",
    { Message::AckMessage{
        { Message::ServerMessageType::LEAVE_ACK }, 1, Message::NackReason::UNSET } },
    {} };

static const TestCase kSingleDisconnect{
    "SingleDisconnect",
    { Message::DisconnectMessage{ { Message::ServerMessageType::DISCONNECT },
                                  "reason" } },
    {} };

static const TestCase kSingleDeath{
    "SingleDeath",
    { Message::DeathMessage{ { Message::ServerMessageType::DEATH }, 42 } },
    {} };

static const TestCase kSingleSnapshot{
    "SingleSnapshot",
    { Message::SnapshotMessage{
        { Message::ServerMessageType::SNAPSHOT },
        Snapshot{ { makeSnake( 1, { makeCoord( 1, 1 ), makeCoord( 1, 2 ) } ) },
                  { makeCoord( 5, 5 ) } } } },
    {} };

// --- Pure egress (client -> server) ---

static const TestCase kSingleRegister{
    "SingleRegister",
    {},
    { Message::RegisterMessage{
        { Message::ClientMessageType::REGISTER }, 0, "Alice" } } };

static const TestCase kSingleRegisterExistingId{
    "SingleRegisterExistingId",
    {},
    { Message::RegisterMessage{
        { Message::ClientMessageType::REGISTER }, 7, "Bob" } } };

static const TestCase kSingleJoin{
    "SingleJoin",
    {},
    { Message::JoinMessage{ { Message::ClientMessageType::JOIN }, 99999 } } };

static const TestCase kSingleChangeDir{
    "SingleChangeDir",
    {},
    { Message::ChangeDirMessage{ { Message::ClientMessageType::CHANGE_DIR },
                                 Move::left } } };

static const TestCase kSingleLeave{
    "SingleLeave",
    {},
    { Message::LeaveMessage{ { Message::ClientMessageType::LEAVE } } } };

// --- Ingress edge cases ---

static const TestCase kEmptyDisconnectReason{
    "EmptyDisconnectReason",
    { Message::DisconnectMessage{ { Message::ServerMessageType::DISCONNECT }, "" } },
    {} };

static const TestCase kZeroScore{
    "ZeroScore",
    { Message::DeathMessage{ { Message::ServerMessageType::DEATH }, 0 } },
    {} };

static const TestCase kMaxScore{
    "MaxScore",
    { Message::DeathMessage{ { Message::ServerMessageType::DEATH },
                             std::numeric_limits< uint32_t >::max() } },
    {} };

static const TestCase kEmptySnapshot{
    "EmptySnapshot",
    { Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT }, {} } },
    {} };

// --- Egress edge cases ---

static const TestCase kRegisterEmptyName{
    "RegisterEmptyName",
    {},
    { Message::RegisterMessage{
        { Message::ClientMessageType::REGISTER }, 0, "" } } };

static const TestCase kChangeDirAllDirections{
    "ChangeDirAllDirections",
    {},
    { Message::ChangeDirMessage{ { Message::ClientMessageType::CHANGE_DIR },
                                 Move::left },
      Message::ChangeDirMessage{ { Message::ClientMessageType::CHANGE_DIR },
                                 Move::right },
      Message::ChangeDirMessage{ { Message::ClientMessageType::CHANGE_DIR },
                                 Move::up },
      Message::ChangeDirMessage{ { Message::ClientMessageType::CHANGE_DIR },
                                 Move::down },
      Message::ChangeDirMessage{ { Message::ClientMessageType::CHANGE_DIR },
                                 Move::stay } } };

// --- Multi-message, single-direction ---

static const TestCase kRegisterThenJoinAck{
    "RegisterThenJoinAck",
    { Message::AckMessage{ { Message::ServerMessageType::REGISTER_ACK },
                           1,
                           Message::NackReason::UNSET },
      Message::AckMessage{ { Message::ServerMessageType::JOIN_ACK },
                           1,
                           Message::NackReason::UNSET } },
    {} };

static const TestCase kSnapshotBurst{
    "SnapshotBurst",
    { Message::SnapshotMessage{
          { Message::ServerMessageType::SNAPSHOT },
          Snapshot{ { makeSnake( 1, { makeCoord( 0, 0 ) } ) }, {} } },
      Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT },
                                Snapshot{ {}, { makeCoord( 2, 2 ) } } },
      Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT },
                                Snapshot{ { makeSnake( 2, { makeCoord( 9, 9 ) } ) },
                                          { makeCoord( 1, 1 ) } } } },
    {} };

static const TestCase kRegisterThenJoin{
    "RegisterThenJoin",
    {},
    { Message::RegisterMessage{
          { Message::ClientMessageType::REGISTER }, 0, "Carol" },
      Message::JoinMessage{ { Message::ClientMessageType::JOIN }, 5 } } };

// --- Combined ingress + egress ---

static const TestCase kFullSessionFlow{
    "FullSessionFlow",
    { Message::AckMessage{ { Message::ServerMessageType::REGISTER_ACK },
                           1,
                           Message::NackReason::UNSET },
      Message::AckMessage{
          { Message::ServerMessageType::JOIN_ACK }, 1, Message::NackReason::UNSET },
      Message::AckMessage{ { Message::ServerMessageType::CHANGE_DIR_ACK },
                           1,
                           Message::NackReason::UNSET },
      Message::DeathMessage{ { Message::ServerMessageType::DEATH }, 100 },
      Message::AckMessage{ { Message::ServerMessageType::LEAVE_ACK },
                           1,
                           Message::NackReason::UNSET } },
    { Message::RegisterMessage{
          { Message::ClientMessageType::REGISTER }, 0, "Dave" },
      Message::JoinMessage{ { Message::ClientMessageType::JOIN }, 1 },
      Message::ChangeDirMessage{ { Message::ClientMessageType::CHANGE_DIR },
                                 Move::up },
      Message::LeaveMessage{ { Message::ClientMessageType::LEAVE } } } };

static const TestCase kAckThenDisconnect_RegisterThenLeave{
    "AckThenDisconnect_RegisterThenLeave",
    { Message::AckMessage{
          { Message::ServerMessageType::JOIN_ACK }, 1, Message::NackReason::UNSET },
      Message::DisconnectMessage{ { Message::ServerMessageType::DISCONNECT },
                                  "idle" } },
    { Message::RegisterMessage{
          { Message::ClientMessageType::REGISTER }, 0, "Erin" },
      Message::LeaveMessage{ { Message::ClientMessageType::LEAVE } } } };

static const TestCase kSnapshotWhileChangingDir{
    "SnapshotWhileChangingDir",
    { Message::SnapshotMessage{ { Message::ServerMessageType::SNAPSHOT },
                                Snapshot{ { makeSnake( 1, { makeCoord( 3, 3 ) } ) },
                                          { makeCoord( 4, 4 ) } } } },
    { Message::ChangeDirMessage{ { Message::ClientMessageType::CHANGE_DIR },
                                 Move::down } } };

} // namespace

static const std::vector< TestCase > messages{
    kSingleRegisterAck,
    kSingleJoinAck,
    kSingleChangeDirAck,
    kSingleLeaveAck,
    kSingleDisconnect,
    kSingleDeath,
    kSingleSnapshot,
    kSingleRegister,
    kSingleRegisterExistingId,
    kSingleJoin,
    kSingleChangeDir,
    kSingleLeave,
    kEmptyDisconnectReason,
    kZeroScore,
    kMaxScore,
    kEmptySnapshot,
    kRegisterEmptyName,
    kChangeDirAllDirections,
    kRegisterThenJoinAck,
    kSnapshotBurst,
    kRegisterThenJoin,
    kFullSessionFlow,
    kAckThenDisconnect_RegisterThenLeave,
    kSnapshotWhileChangingDir,
};

INSTANTIATE_TEST_SUITE_P( BasicClientPipelineTest,
                          ClientIntegrationTest,
                          ::testing::ValuesIn( messages ),
                          []( const testing::TestParamInfo< TestCase > & test ) {
                             return test.param.name;
                          } );
