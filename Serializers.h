#pragma once
#include "BytesOperator.h"
#include "Messages.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <meta>
#include <optional>
#include <type_traits>
#include <vector>

namespace {
template < typename T, std::meta::info member >
consteval std::optional< T >
get_annotation() {
   constexpr auto annotations = std::define_static_array(
       std::meta::annotations_of_with_type( member, ^^T ) );
   if constexpr ( annotations.empty() ) {
      return std::nullopt;
   } else {
      static_assert(
          annotations.size() <= 1,
          "Cannot have multiple annotations of the same type on the same member" );
      constexpr auto annotation = annotations[ 0 ];
      return std::optional< T >( std::meta::extract< T >( annotation ) );
   }
   return std::nullopt;
}
} // namespace

namespace Serdes {

using namespace Message;

template < typename MessageT, typename BytesOp >
void
transfer( MessageT && msg, BytesOp & bytesOp ) {
   using DeducedType = std::remove_reference_t< MessageT >;
   using MessageType = std::remove_cvref_t< MessageT >;

   static constexpr auto bases = std::define_static_array(
       std::meta::bases_of( ^^MessageType, std::meta::access_context::current() ) );
   template for ( constexpr auto b : bases ) {
      constexpr auto baseTypeMeta = std::meta::type_of( b );
      // We need to ensure to keep const-correct here
      using BaseType = typename[:baseTypeMeta:];
      if constexpr ( std::is_const_v< DeducedType > ) {
         transfer( static_cast< const BaseType & >( msg ), bytesOp );
      } else {
         transfer( static_cast< BaseType & >( msg ), bytesOp );
      }
   }

   static constexpr auto members =
       std::define_static_array( std::meta::nonstatic_data_members_of(
           ^^MessageType, std::meta::access_context::current() ) );
   template for ( constexpr auto m : members ) {
      if constexpr ( constexpr auto serializableOpt =
                         get_annotation< Message::Annotation::Serialize, m >();
                     serializableOpt.has_value() &&
                     !serializableOpt.value().shouldSerialize ) {
         continue;
      }
      transfer( msg.[:m:], bytesOp );
   }
}

template < TriviallySerializable MessageT, typename BytesOp >
void
transfer( MessageT && msg, BytesOp & bytesOp ) {
   bytesOp.template transfer( msg );
}

template < ContiguousDynamicallySized MessageT, typename BytesOp >
void
transfer( MessageT && msg, BytesOp & bytesOp ) {
   using LengthType = std::uint16_t;
   auto collLen = static_cast< LengthType >( msg.size() );
   bytesOp.template transfer( collLen );
   bytesOp.template transfer( msg );
}

template < ContiguousDynamicallySized MessageT >
void
transfer( MessageT && msg, BytesReader & bytesReader ) {
   using LengthType = std::uint16_t;
   auto collLen = static_cast< LengthType >( msg.size() );
   bytesReader.template transfer( collLen );
   msg.resize( collLen );
   bytesReader.template transfer( msg );
}

} // namespace Serdes
