#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>

#include "Messages.h"
#include "Serdes.h"

namespace Serdes {

using namespace Message;

// Keeping these generic and instantiating with BytesCounter or BytesWriter prevents
// our size calculation and serialization going out of sync
template < typename BytesOp >
void
write( const RegisterMessage & msg, BytesOp & bytesOp ) {
   bytesOp.write( msg.msgType );
   bytesOp.write( msg.existingClientId );
   bytesOp.write( msg.nameLen );
   bytesOp.writeBytes( msg.name.data(), msg.nameLen );
}

inline void
read( RegisterMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ClientMessageType >();
   msg.existingClientId = reader.read< ClientId >();
   using NameLen = decltype( RegisterMessage::nameLen );
   msg.nameLen = reader.read< NameLen >();
   msg.name.resize( msg.nameLen );
   reader.readBytes< char >( msg.name );
}

template < typename BytesOp >
void
write( const JoinMessage msg, BytesOp & bytesOp ) {
   bytesOp.write( msg.msgType );
   bytesOp.write( msg.roomId );
}

inline void
read( JoinMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ClientMessageType >();
   msg.roomId = reader.read< std::uint32_t >();
}

template < typename BytesOp >
void
write( const ChangeDirMessage msg, BytesOp & bytesOp ) {
   bytesOp.write( msg.msgType );
   bytesOp.write( msg.newDir );
}

inline void
read( ChangeDirMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ClientMessageType >();
   msg.newDir = reader.read< Move >();
}

template < typename BytesOp >
void
write( const LeaveMessage msg, BytesOp & bytesOp ) {
   bytesOp.write( msg.msgType );
}

inline void
read( LeaveMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ClientMessageType >();
}

template < typename BytesOp >
void
write( const AckMessage msg, BytesOp & bytesOp ) {
   bytesOp.write( msg.msgType );
   bytesOp.write( msg.id );
   bytesOp.write( msg.reason );
}

inline void
read( AckMessage & ack, BytesReader & reader ) {
   ack.msgType = reader.read< ServerMessageType >();
   ack.id = reader.read< ClientId >();
   ack.reason = reader.read< NackReason >();
}

template < typename BytesOp >
void
write( const DisconnectMessage & msg, BytesOp & bytesOp ) {
   bytesOp.write( msg.msgType );
   bytesOp.write( msg.reasonLen );
   bytesOp.writeBytes( msg.reason.data(), msg.reasonLen );
}

inline void
read( DisconnectMessage & msg, BytesReader & reader ) {
   using MsgLen = decltype( DisconnectMessage::reasonLen );
   msg.msgType = reader.read< ServerMessageType >();
   msg.reasonLen = reader.read< MsgLen >();
   msg.reason.resize( msg.reasonLen );
   reader.readBytes< char >( msg.reason );
}

template < typename BytesOp >
void
write( const DeathMessage msg, BytesOp & bytesOp ) {
   bytesOp.write( msg.msgType );
   bytesOp.write( msg.score );
}

inline void
read( DeathMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ServerMessageType >();
   msg.score = reader.read< uint32_t >();
}

template < typename BytesOp >
void
write( const SnapshotMessage & msg, BytesOp & bytesOp ) {
   bytesOp.write( msg.msgType );
   bytesOp.write( msg.snapshotLen );
   bytesOp.writeBytes( msg.bytes.data(), msg.snapshotLen );
}

inline void
read( SnapshotMessage & msg, BytesReader & reader ) {
   using MsgLen = decltype( SnapshotMessage::snapshotLen );
   msg.msgType = reader.read< ServerMessageType >();
   msg.snapshotLen = reader.read< MsgLen >();
   msg.bytes.resize( msg.snapshotLen );
   reader.readBytes< std::uint8_t >( msg.bytes );
}

} // namespace Serdes
