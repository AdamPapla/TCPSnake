#pragma once
#include "MessageTraits.h"
#include "Messages.h"
#include "Serializers.h"

namespace SnakeSerdes {

template < typename Message >
static std::optional< Message >
readNext( Serdes::BytesReader & reader ) {
   using namespace Message;
   auto msgLen = reader.try_read< std::uint32_t >();
   if ( !msgLen || reader.remainingBytes() < msgLen.value() ) {
      return std::nullopt;
   }
   auto msgType = reader.peek< typename MessageTraits< Message >::Type >();
   return MessageTraits< Message >::read( msgType, reader );
}

template < typename Message >
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

} // namespace SnakeSerdes
