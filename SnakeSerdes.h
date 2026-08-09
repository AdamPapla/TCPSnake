#include "Messages.h"
#include "Serializers.h"

namespace SnakeSerdes {

template < typename T >
static T
readAs( Serdes::BytesReader & reader ) {
   T msg;
   Serdes::transfer( msg, reader );
   return msg;
}

static std::optional< Message::ServerMessage >
readNextServerMsg( Serdes::BytesReader & reader ) {
   using namespace Message;
   auto msgLen = reader.try_read< std::uint32_t >();
   if ( !msgLen || reader.remainingBytes() < msgLen.value() ) {
      return std::nullopt;
   }
   auto msgType = reader.peek< Message::ServerMessageType >();
   switch ( msgType ) {
   case ServerMessageType::REGISTER_ACK:
   case ServerMessageType::JOIN_ACK:
   case ServerMessageType::CHANGE_DIR_ACK:
   case ServerMessageType::LEAVE_ACK:
      return readAs< AckMessage >( reader );
   case ServerMessageType::SNAPSHOT:
      return readAs< SnapshotMessage >( reader );
   case ServerMessageType::DISCONNECT:
      return readAs< DisconnectMessage >( reader );
   case ServerMessageType::DEATH:
      return readAs< DeathMessage >( reader );
      return std::nullopt;
   }
   return std::nullopt;
}

static std::optional< Message::ClientMessage >
readNextClientMsg( Serdes::BytesReader & reader ) {
   using namespace Message;
   auto msgLen = reader.try_read< std::uint32_t >();
   if ( !msgLen || reader.remainingBytes() < msgLen.value() ) {
      return std::nullopt;
   }
   auto msgType = reader.peek< Message::ClientMessageType >();
   switch ( msgType ) {
   case ClientMessageType::REGISTER:
      return readAs< RegisterMessage >( reader );
   case ClientMessageType::JOIN:
      return readAs< JoinMessage >( reader );
   case ClientMessageType::CHANGE_DIR:
      return readAs< ChangeDirMessage >( reader );
   case ClientMessageType::LEAVE:
      return readAs< LeaveMessage >( reader );
      return std::nullopt;
   }
   return std::nullopt;
}

template< typename Message >
static bool
writeNext( Serdes::BytesWriter & writer, const Message & outbound ) {
   Serdes::BytesCounter counter;
   bool bufferFull = std::visit(
       [ & ]( const auto & msg ) {
          Serdes::transfer( msg, counter );
          // Bail early if we don't have space to write length + payload
          bufferFull =
              sizeof( counter.count ) + counter.count > writer.remainingBytes();
          if ( bufferFull )
             return false;
          Serdes::transfer( counter.count, writer );
          Serdes::transfer( msg, writer );
          return true;
       },
       outbound );
   return true;
}

} // SnakeSerdes
