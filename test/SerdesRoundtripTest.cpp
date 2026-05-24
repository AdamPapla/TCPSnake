#include <cstdint>
#include <gtest/gtest.h>
#include <span>
#include <vector>

#include "BytesOperator.h"
#include "Messages.h"
#include "Serializers.h"

// Serialize a message to bytes then deserialize back, returning a reader over the
// buffer. The caller holds the buffer and passes the reader to their read() call.
template < typename Msg >
std::pair< std::vector< std::uint8_t >, Serdes::BytesReader >
makeRoundtrip( const Msg & msg ) {
   Serdes::BytesCounter counter;
   Serdes::write( msg, counter );

   std::vector< std::uint8_t > buf( counter.count );
   Serdes::BytesWriter writer{ std::span< std::uint8_t >{ buf } };
   Serdes::write( msg, writer );

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
   original.nameLen = 5;
   original.name = "Alice";

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::RegisterMessage result;
   Serdes::read( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.existingClientId, original.existingClientId );
   EXPECT_EQ( result.nameLen, original.nameLen );
   EXPECT_EQ( result.name, original.name );
}

TEST( SerdesRoundtrip, RegisterMessageNoExistingId ) {
   Message::RegisterMessage original;
   original.msgType = Message::ClientMessageType::REGISTER;
   original.existingClientId = 0;
   original.nameLen = 3;
   original.name = "Bob";

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::RegisterMessage result;
   Serdes::read( result, reader );
   EXPECT_EQ( result.existingClientId, 0 );
   EXPECT_EQ( result.name, "Bob" );
}

TEST( SerdesRoundtrip, JoinMessage ) {
   Message::JoinMessage original;
   original.msgType = Message::ClientMessageType::JOIN;
   original.roomId = 99999;

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::JoinMessage result;
   Serdes::read( result, reader );
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
      Serdes::read( result, reader );
      EXPECT_EQ( result.msgType, original.msgType );
      EXPECT_EQ( result.newDir, dir );
   }
}

TEST( SerdesRoundtrip, LeaveMessage ) {
   Message::LeaveMessage original;
   original.msgType = Message::ClientMessageType::LEAVE;

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::LeaveMessage result;
   Serdes::read( result, reader );
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
      Serdes::read( result, reader );
      EXPECT_EQ( result.msgType, original.msgType );
      EXPECT_EQ( result.id, original.id );
      EXPECT_EQ( result.reason, original.reason );
   }
}

TEST( SerdesRoundtrip, DisconnectMessage ) {
   Message::DisconnectMessage original;
   original.msgType = Message::ServerMessageType::DISCONNECT;
   original.reason = "Server shutting down";
   original.reasonLen = static_cast< std::uint16_t >( original.reason.size() );

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::DisconnectMessage result;
   Serdes::read( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.reasonLen, original.reasonLen );
   EXPECT_EQ( result.reason, original.reason );
}

TEST( SerdesRoundtrip, DisconnectMessageEmptyReason ) {
   Message::DisconnectMessage original;
   original.msgType = Message::ServerMessageType::DISCONNECT;
   original.reason = "";
   original.reasonLen = 0;

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::DisconnectMessage result;
   Serdes::read( result, reader );
   EXPECT_EQ( result.reasonLen, 0 );
   EXPECT_EQ( result.reason, "" );
}

TEST( SerdesRoundtrip, DeathMessage ) {
   Message::DeathMessage original;
   original.msgType = Message::ServerMessageType::DEATH;
   original.score = 1337;

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::DeathMessage result;
   Serdes::read( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.score, original.score );
}

TEST( SerdesRoundtrip, SnapshotMessage ) {
   Message::SnapshotMessage original;
   original.msgType = Message::ServerMessageType::SNAPSHOT;
   original.bytes = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02 };
   original.snapshotLen = static_cast< std::uint32_t >( original.bytes.size() );

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::SnapshotMessage result;
   Serdes::read( result, reader );
   EXPECT_EQ( result.msgType, original.msgType );
   EXPECT_EQ( result.snapshotLen, original.snapshotLen );
   EXPECT_EQ( result.bytes, original.bytes );
}

TEST( SerdesRoundtrip, SnapshotMessageEmpty ) {
   Message::SnapshotMessage original;
   original.msgType = Message::ServerMessageType::SNAPSHOT;
   original.bytes = {};
   original.snapshotLen = 0;

   auto [ buf, reader ] = makeRoundtrip( original );

   Message::SnapshotMessage result;
   Serdes::read( result, reader );
   EXPECT_EQ( result.snapshotLen, 0 );
   EXPECT_TRUE( result.bytes.empty() );
}

// ---------------------------------------------------------------------------
// BytesOperator contract tests
// ---------------------------------------------------------------------------

TEST( BytesOperator, WriterThrowsOnOverflow ) {
   std::vector< std::uint8_t > buf( 1 );
   Serdes::BytesWriter writer{ std::span< std::uint8_t >{ buf } };
   EXPECT_THROW( writer.write( std::uint32_t{ 42 } ), std::runtime_error );
}

TEST( BytesOperator, ReaderThrowsOnUnderflow ) {
   std::vector< std::uint8_t > buf( 1 );
   Serdes::BytesReader reader{ std::span< std::uint8_t >{ buf } };
   EXPECT_THROW( reader.read< std::uint32_t >(), std::runtime_error );
}

TEST( BytesOperator, CounterMatchesWriterConsumption ) {
   Message::RegisterMessage msg;
   msg.msgType = Message::ClientMessageType::REGISTER;
   msg.existingClientId = 1;
   msg.nameLen = 4;
   msg.name = "Test";

   Serdes::BytesCounter counter;
   Serdes::write( msg, counter );

   std::vector< std::uint8_t > buf( counter.count );
   Serdes::BytesWriter writer{ std::span< std::uint8_t >{ buf } };
   Serdes::write( msg, writer );

   EXPECT_EQ( writer.remainingBytes(), 0u );
}
