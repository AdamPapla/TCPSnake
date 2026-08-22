#pragma once
#include "BytesOperator.h"
#include "Messages.h"
#include "Serializers.h"

namespace Message {

template < typename Message >
struct MessageTraits;

template <>
struct MessageTraits< ClientMessage > {
   using Type = ClientMessageType;
   static std::optional< Message::ClientMessage >
   read( Type type, Serdes::BytesReader & reader ) {
      switch ( type ) {
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
};

template <>
struct MessageTraits< ServerMessage > {
   using Type = ServerMessageType;
   static std::optional< Message::ServerMessage >
   read( Type type, Serdes::BytesReader & reader ) {
      switch ( type ) {
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
};
} // namespace Message
