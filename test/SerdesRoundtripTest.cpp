#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "BytesOperator.h"
#include "Messages.h"
#include "Serializers.h"

// Serialize a message to bytes then deserialize back, returning a reader over the
// buffer. The caller holds the buffer and passes the reader to their transfer()
// call.
template < typename Msg >
std::pair< std::vector< std::uint8_t >, Serdes::BytesReader >
makeRoundtrip( const Msg & msg ) {
   Serdes::BytesCounter counter;
   Serdes::transfer( msg, counter );

   std::vector< std::uint8_t > buf( counter.count );
   Serdes::BytesWriter writer{ std::span< std::uint8_t >{ buf } };
   Serdes::transfer( msg, writer );

   Serdes::BytesReader reader{ std::span< std::uint8_t >{ buf } };
   return { std::move( buf ), reader };
}

// ---------------------------------------------------------------------------
// Client messages
// ---------------------------------------------------------------------------

TEST( SerdesRoundtrip, RegisterMessage ) {
   Message::RegisterMessage original;
   original.msgType = Message::ClientMessageType::REGISTER;
   original.existingClientId = 42;
   original.name = "Alice";

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::RegisterMessage result;
   Serdes::transfer( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.existingClientId, original.existingClientId );
   EXPECT_EQ( result.name, original.name );
}

TEST( SerdesRoundtrip, RegisterMessageNoExistingId ) {
   Message::RegisterMessage original;
   original.msgType = Message::ClientMessageType::REGISTER;
   original.existingClientId = 0;
   original.name = "Bob";

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::RegisterMessage result;
   Serdes::transfer( result, reader );
   EXPECT_EQ( result.existingClientId, 0 );
   EXPECT_EQ( result.name, "Bob" );
}

TEST( SerdesRoundtrip, JoinMessage ) {
   Message::JoinMessage original;
   original.msgType = Message::ClientMessageType::JOIN;
   original.roomId = 99999;

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::JoinMessage result;
   Serdes::transfer( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.roomId, original.roomId );
}

TEST( SerdesRoundtrip, ChangeDirMessage ) {
   for ( const auto dir :
         { Move::left, Move::right, Move::up, Move::down, Move::stay } ) {
      Message::ChangeDirMessage original;
      original.msgType = Message::ClientMessageType::CHANGE_DIR;
      original.newDir = dir;

      auto [ buf, reader ] = makeRoundtrip( original );

      Message::ChangeDirMessage result;
      Serdes::transfer( result, reader );
      EXPECT_EQ( result.msgType, original.msgType );
      EXPECT_EQ( result.newDir, dir );
   }
}

TEST( SerdesRoundtrip, LeaveMessage ) {
   Message::LeaveMessage original;
   original.msgType = Message::ClientMessageType::LEAVE;

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::LeaveMessage result;
   Serdes::transfer( result, reader );
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

      auto [ buf, reader ] = makeRoundtrip( original );

      Message::AckMessage result;
      Serdes::transfer( result, reader );
      EXPECT_EQ( result.msgType, original.msgType );
      EXPECT_EQ( result.id, original.id );
      EXPECT_EQ( result.reason, original.reason );
   }
}

TEST( SerdesRoundtrip, DisconnectMessage ) {
   Message::DisconnectMessage original;
   original.msgType = Message::ServerMessageType::DISCONNECT;
   original.reason = "Server shutting down";

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::DisconnectMessage result;
   Serdes::transfer( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.reason, original.reason );
}

TEST( SerdesRoundtrip, DisconnectMessageEmptyReason ) {
   Message::DisconnectMessage original;
   original.msgType = Message::ServerMessageType::DISCONNECT;
   original.reason = "";

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::DisconnectMessage result;
   Serdes::transfer( result, reader );
   EXPECT_EQ( result.reason, "" );
}

TEST( SerdesRoundtrip, DeathMessage ) {
   Message::DeathMessage original;
   original.msgType = Message::ServerMessageType::DEATH;
   original.score = 1337;

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::DeathMessage result;
   Serdes::transfer( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.score, original.score );
}

TEST( SerdesRoundtrip, SnapshotMessage ) {
   Message::SnapshotMessage original;
   original.msgType = Message::ServerMessageType::SNAPSHOT;
   original.bytes = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::SnapshotMessage result;
   Serdes::transfer( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.bytes, original.bytes );
}

TEST( SerdesRoundtrip, SnapshotMessageEmpty ) {
   Message::SnapshotMessage original;
   original.msgType = Message::ServerMessageType::SNAPSHOT;
   original.bytes = {};

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::SnapshotMessage result;
   Serdes::transfer( result, reader );
   EXPECT_TRUE( result.bytes.empty() );
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
