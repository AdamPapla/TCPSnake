#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <meta>
#include <vector>

#include "Messages.h"
#include "Serdes.h"

namespace Serdes {

using namespace Message;

template < typename Message, typename BytesOp >
void
write( const Message & msg, BytesOp & bytesOp ) {
   constexpr auto type = ^^Message;
   // Recurse into base classes
   static constexpr auto bases = define_static_array(
       std::meta::bases_of( ^^Message, std::meta::access_context::current() ) );
   template for ( constexpr auto b : bases ) {
      constexpr auto baseType = std::meta::type_of( b );
      write( static_cast< typename[:baseType:] >( msg ), bytesOp );
   }
   // Then write members
   static constexpr auto members =
       define_static_array( std::meta::nonstatic_data_members_of(
           ^^Message, std::meta::access_context::current() ) );
   template for ( constexpr auto m : members ) {
      constexpr auto memberType = std::meta::type_of( m );
      if constexpr ( TriviallySerializable< typename[:memberType:] > ||
                     ContiguousDynamicallySized< typename[:memberType:] > ) {
         bytesOp.write( msg.[:m:] );
      } else {
         // If we don't have a write method for the member, recurse. Note this will
         // break if there's e.g. maps in the type closure
         write( msg.[:m:], bytesOp );
      }
   }
}

inline void
read( RegisterMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ClientMessageType >();
   msg.existingClientId = reader.read< ClientId >();
   using NameLen = decltype( RegisterMessage::nameLen );
   msg.nameLen = reader.read< NameLen >();
   msg.name.resize( msg.nameLen );
   reader.read( msg.name );
}

inline void
read( JoinMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ClientMessageType >();
   msg.roomId = reader.read< std::uint32_t >();
}

inline void
read( ChangeDirMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ClientMessageType >();
   msg.newDir = reader.read< Move >();
}

inline void
read( LeaveMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ClientMessageType >();
}

inline void
read( AckMessage & ack, BytesReader & reader ) {
   ack.msgType = reader.read< ServerMessageType >();
   ack.id = reader.read< ClientId >();
   ack.reason = reader.read< NackReason >();
}

inline void
read( DisconnectMessage & msg, BytesReader & reader ) {
   using MsgLen = decltype( DisconnectMessage::reasonLen );
   msg.msgType = reader.read< ServerMessageType >();
   msg.reasonLen = reader.read< MsgLen >();
   msg.reason.resize( msg.reasonLen );
   reader.read< std::string >( msg.reason );
}

inline void
read( DeathMessage & msg, BytesReader & reader ) {
   msg.msgType = reader.read< ServerMessageType >();
   msg.score = reader.read< uint32_t >();
}

inline void
read( SnapshotMessage & msg, BytesReader & reader ) {
   using MsgLen = decltype( SnapshotMessage::snapshotLen );
   msg.msgType = reader.read< ServerMessageType >();
   msg.snapshotLen = reader.read< MsgLen >();
   msg.bytes.resize( msg.snapshotLen );
   reader.read( msg.bytes );
}

} // namespace Serdes
