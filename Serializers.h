#pragma once
#include "BytesOperator.h"
#include "Messages.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
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
   using Deduced = std::remove_reference_t< MessageT >;
   using UnqMessage = std::remove_cvref_t< MessageT >;

   static constexpr auto bases = std::define_static_array(
       std::meta::bases_of( ^^UnqMessage, std::meta::access_context::current() ) );
   template for ( constexpr auto b : bases ) {
      constexpr auto baseTypeMeta = std::meta::type_of( b );
      // We need to ensure to keep const-correct here
      using Base = typename[:baseTypeMeta:];
      if constexpr ( std::is_const_v< Deduced > ) {
         transfer( static_cast< const Base & >( msg ), bytesOp );
      } else {
         transfer( static_cast< Base & >( msg ), bytesOp );
      }
   }

   static constexpr auto members =
       std::define_static_array( std::meta::nonstatic_data_members_of(
           ^^UnqMessage, std::meta::access_context::current() ) );
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
   using Length = std::uint16_t;
   auto collLen = static_cast< Length >( msg.size() );
   transfer( collLen, bytesOp );
   bytesOp.template transfer( msg );
}

template < ContiguousDynamicallySized MessageT >
void
transfer( MessageT && msg, BytesReader & bytesReader ) {
   using Length = std::uint16_t;
   auto collLen = static_cast< Length >( msg.size() );
   transfer( collLen, bytesReader );
   msg.resize( collLen );
   bytesReader.template transfer( msg );
}

template < PushBackColl MessageT, typename BytesOp >
void
transfer( MessageT && msg, BytesOp & bytesOp ) {
   using Length = std::uint16_t;
   auto collLen = static_cast< Length >( msg.size() );
   transfer( collLen, bytesOp );
   for ( auto && elem : msg ) {
      transfer( elem, bytesOp );
   }
}

template < PushBackColl MessageT >
void
transfer( MessageT && msg, BytesReader & bytesReader ) {
   using Length = std::uint16_t;
   using Elem = std::ranges::range_value_t< std::remove_cvref_t< MessageT > >;
   Length collLen;
   transfer( collLen, bytesReader );
   Elem elem;
   for ( int i = 0; i < collLen; ++i ) {
      transfer( elem, bytesReader );
      msg.push_back( std::move( elem ) );
   }
}

template < typename T >
static T
readAs( Serdes::BytesReader & reader ) {
   T msg;
   Serdes::transfer( msg, reader );
   return msg;
}

} // namespace Serdes
