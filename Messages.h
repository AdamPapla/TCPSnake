#pragma once
#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <variant>
#include <vector>

#include "Utility.h"

namespace Message {

namespace Annotation {

struct Serialize {
   bool shouldSerialize{ true };
};

} // namespace Annotation
enum class ClientMessageType : std::uint16_t { REGISTER, JOIN, CHANGE_DIR, LEAVE };

struct ClientMessageBase {
   ClientMessageType msgType;
   bool operator==( const ClientMessageBase & ) const = default;
};

struct RegisterMessage : ClientMessageBase {
   ClientId existingClientId; // 0 is sentinel/null value
   std::string name;
   bool operator==( const RegisterMessage & ) const = default;
};

struct JoinMessage : ClientMessageBase {
   std::uint32_t roomId;
   bool operator==( const JoinMessage & ) const = default;
};

struct ChangeDirMessage : ClientMessageBase {
   Move newDir;
   bool operator==( const ChangeDirMessage & ) const = default;
};

struct LeaveMessage : ClientMessageBase {
   bool operator==( const LeaveMessage & ) const = default;
};

enum class ServerMessageType : std::uint16_t {
   // Ack/Nack client messages
   REGISTER_ACK,
   JOIN_ACK,
   CHANGE_DIR_ACK,
   LEAVE_ACK,

   // Server response messages
   DISCONNECT,
   DEATH,
   SNAPSHOT
};

struct ServerMessageBase {
   ServerMessageType msgType;
   bool operator==( const ServerMessageBase & ) const = default;
};

enum class NackReason : std::uint16_t {
   // TODO: This will be expanded as needed, placeholder for now
   UNSET
};

struct AckMessage : ServerMessageBase {
   ClientId id;
   NackReason reason;
   bool operator==( const AckMessage & ) const = default;
};

struct DisconnectMessage : ServerMessageBase {
   std::string reason;
   bool operator==( const DisconnectMessage & ) const = default;
};

struct DeathMessage : ServerMessageBase {
   uint32_t score;
   bool operator==( const DeathMessage & ) const = default;
};

struct SnapshotMessage : ServerMessageBase {
   std::vector< std::uint8_t > bytes;
   bool operator==( const SnapshotMessage & ) const = default;
};

using ClientMessage =
    std::variant< RegisterMessage, JoinMessage, ChangeDirMessage, LeaveMessage >;
using ServerMessage =
    std::variant< AckMessage, DisconnectMessage, DeathMessage, SnapshotMessage >;

} // namespace Message
