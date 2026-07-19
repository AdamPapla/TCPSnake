#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "BytesOperator.h"
#include "Messages.h"
#include "Serializers.h"

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

// Serialize a message to bytes then deserialize back, returning a reader over the
// buffer. The caller holds the buffer and passes the reader to their transfer()
// call.
template < typename Msg >
Msg
makeRoundtrip( const Msg & msg ) {
   Serdes::BytesCounter counter;
   Serdes::transfer( msg, counter );

   std::vector< std::uint8_t > buf( counter.count );
   Serdes::BytesWriter writer{ std::span< std::uint8_t >{ buf } };
   Serdes::transfer( msg, writer );

   Msg readMsg;
   Serdes::BytesReader reader{ std::span< std::uint8_t >{ buf } };
   transfer( readMsg, reader );
   return readMsg;
}

} // namespace
// ---------------------------------------------------------------------------
// Client messages
// ---------------------------------------------------------------------------

TEST( SerdesRoundtrip, RegisterMessage ) {
   Message::RegisterMessage original;
   original.msgType = Message::ClientMessageType::REGISTER;
   original.existingClientId = 42;
   original.name = "Alice";

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.existingClientId, original.existingClientId );
   EXPECT_EQ( result.name, original.name );
}

TEST( SerdesRoundtrip, RegisterMessageNoExistingId ) {
   Message::RegisterMessage original;
   original.msgType = Message::ClientMessageType::REGISTER;
   original.existingClientId = 0;
   original.name = "Bob";

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.existingClientId, 0 );
   EXPECT_EQ( result.name, "Bob" );
}

TEST( SerdesRoundtrip, JoinMessage ) {
   Message::JoinMessage original;
   original.msgType = Message::ClientMessageType::JOIN;
   original.roomId = 99999;

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.roomId, original.roomId );
}

TEST( SerdesRoundtrip, ChangeDirMessage ) {
   for ( const auto dir :
         { Move::left, Move::right, Move::up, Move::down, Move::stay } ) {
      Message::ChangeDirMessage original;
      original.msgType = Message::ClientMessageType::CHANGE_DIR;
      original.newDir = dir;

      auto result = makeRoundtrip( original );

      EXPECT_EQ( result.msgType, original.msgType );
      EXPECT_EQ( result.newDir, dir );
   }
}

TEST( SerdesRoundtrip, LeaveMessage ) {
   Message::LeaveMessage original;
   original.msgType = Message::ClientMessageType::LEAVE;

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
}

// ---------------------------------------------------------------------------
// Server messages
// ---------------------------------------------------------------------------

TEST( SerdesRoundtrip, AckMessage ) {
   for ( const auto type : {
             Message::ServerMessageType::REGISTER_ACK,
             Message::ServerMessageType::JOIN_ACK,
             Message::ServerMessageType::CHANGE_DIR_ACK,
             Message::ServerMessageType::LEAVE_ACK,
         } ) {
      Message::AckMessage original;
      original.msgType = type;
      original.id = 7;
      original.reason = Message::NackReason::UNSET;

      auto result = makeRoundtrip( original );

      EXPECT_EQ( result.msgType, original.msgType );
      EXPECT_EQ( result.id, original.id );
      EXPECT_EQ( result.reason, original.reason );
   }
}

TEST( SerdesRoundtrip, DisconnectMessage ) {
   Message::DisconnectMessage original;
   original.msgType = Message::ServerMessageType::DISCONNECT;
   original.reason = "Server shutting down";

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.reason, original.reason );
}

TEST( SerdesRoundtrip, DisconnectMessageEmptyReason ) {
   Message::DisconnectMessage original;
   original.msgType = Message::ServerMessageType::DISCONNECT;
   original.reason = "";

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.reason, "" );
}

TEST( SerdesRoundtrip, DeathMessage ) {
   Message::DeathMessage original;
   original.msgType = Message::ServerMessageType::DEATH;
   original.score = 1337;

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.score, original.score );
}

TEST( SerdesRoundTrip, EmptySnapshot ) {
   Message::SnapshotMessage original;

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( original.snapshot, result.snapshot );
}

TEST( SerdesRoundTrip, SingleSnakeNoFood ) {
   Message::SnapshotMessage original;

   original.snapshot.snakes.push_back(
       makeSnake( 1, { makeCoord( 1, 1 ), makeCoord( 1, 2 ), makeCoord( 1, 3 ) } ) );

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( original.snapshot, result.snapshot );
}

TEST( SerdesRoundTrip, MultipleSnakesAndFood ) {
   Message::SnapshotMessage original;

   original.snapshot.snakes.push_back(
       makeSnake( 1, { makeCoord( 0, 0 ), makeCoord( 0, 1 ), makeCoord( 0, 2 ) } ) );
   original.snapshot.snakes.push_back(
       makeSnake( 2, { makeCoord( 5, 5 ), makeCoord( 5, 6 ) } ) );
   original.snapshot.food = {
       makeCoord( 10, 10 ), makeCoord( 3, 7 ), makeCoord( 8, 2 ) };

   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( original.snapshot, result.snapshot );
}

TEST( SerdesRoundTrip, LargeSnake ) {
   Message::SnapshotMessage original;

   SnakeSnapshot snake{};
   snake.id = 42;

   for ( uint32_t i = 0; i < 100; ++i ) {
      snake.blocks.push_back( makeCoord( i, i + 1 ) );
   }

   original.snapshot.snakes.push_back( snake );
   auto result = makeRoundtrip( original );

   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( original.snapshot, result.snapshot );
}

// ---------------------------------------------------------------------------
// BytesOperator contract tests
// ---------------------------------------------------------------------------

TEST( BytesOperator, WriterThrowsOnOverflow ) {
   std::vector< std::uint8_t > buf( 1 );
   Serdes::BytesWriter writer{ std::span< std::uint8_t >{ buf } };
   EXPECT_THROW( writer.transfer( std::uint32_t{ 42 } ), std::runtime_error );
}

TEST( BytesOperator, ReaderThrowsOnUnderflow ) {
   std::vector< std::uint8_t > buf( 1 );
   Serdes::BytesReader reader{ std::span< std::uint8_t >{ buf } };
   EXPECT_THROW( reader.transfer< std::uint32_t >(), std::runtime_error );
}

TEST( BytesOperator, CounterMatchesWriterConsumption ) {
   Message::RegisterMessage msg;
   msg.msgType = Message::ClientMessageType::REGISTER;
   msg.existingClientId = 1;
   msg.name = "Test";

   Serdes::BytesCounter counter;
   Serdes::transfer( msg, counter );

   std::vector< std::uint8_t > buf( counter.count );
   Serdes::BytesWriter writer{ std::span< std::uint8_t >{ buf } };
   Serdes::transfer( msg, writer );

   EXPECT_EQ( writer.remainingBytes(), 0u );
}
