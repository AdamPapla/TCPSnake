#pragma once
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <type_traits>

#include "GameState.h"

namespace Serdes {

template < typename T >
concept TriviallySerializable =
    std::is_trivially_copyable_v< std::remove_cvref_t< T > > &&
    std::is_standard_layout_v< std::remove_cvref< T > >;

template < typename Coll >
concept ContiguousDynamicallySized =
    std::ranges::contiguous_range< std::remove_cvref_t< Coll > > &&
    TriviallySerializable<
        std::ranges::range_value_t< std::remove_cvref_t< Coll > > > &&
    !requires { std::tuple_size< std::remove_cvref_t< Coll > >::value; } &&
    requires( Coll c ) {
       { c.size() } -> std::convertible_to< std::size_t >;
    };

struct BytesCounter {
   std::uint32_t count{ 0 };

   template < TriviallySerializable T >
   void transfer( const T & ) {
      count += sizeof( T );
   }
   template < ContiguousDynamicallySized Coll >
   void transfer( const Coll & coll ) {
      using Elem = std::ranges::range_value_t< Coll >;
      count += static_cast< uint32_t >( coll.size() * sizeof( Elem ) );
   }
};

class BytesWriter {
 public:
   BytesWriter( std::span< std::uint8_t > bytes )
       : bytes_{ bytes }, remBytes_{ bytes_.size() } {}

   template < TriviallySerializable T >
   void transfer( const T & val ) {
      if ( remBytes_ < sizeof( T ) ) {
         throw std::runtime_error( "BytesWriter overflow" );
      }
      std::size_t currentIdx = bytes_.size() - remBytes_;
      std::memcpy( bytes_.data() + currentIdx, &val, sizeof( T ) );
      remBytes_ -= sizeof( T );
   }
   template < ContiguousDynamicallySized Coll >
   void transfer( const Coll & coll ) {
      using Elem = std::ranges::range_value_t< Coll >;
      std::size_t currentIdx = bytes_.size() - remBytes_;
      std::size_t writeSize = coll.size() * sizeof( Elem );
      if ( remBytes_ < writeSize ) {
         throw std::runtime_error( "BytesWriter overflow" );
      }
      std::memcpy(
          bytes_.data() + currentIdx, std::ranges::data( coll ), writeSize );
      remBytes_ -= writeSize;
   }
   std::size_t remainingBytes() { return remBytes_; }

 private:
   std::span< uint8_t > bytes_;
   std::size_t remBytes_;
};

struct BytesReader {
 public:
   BytesReader( std::span< std::uint8_t > bytes )
       : bytes_{ bytes }, remBytes_{ bytes.size() } {}

   template < TriviallySerializable T >
   std::optional< T > try_peek() {
      T val;
      if ( remBytes_ < sizeof( T ) ) {
         return std::nullopt;
      }
      std::size_t currentIdx = bytes_.size() - remBytes_;
      std::memcpy( &val, bytes_.data() + currentIdx, sizeof( T ) );
      return val;
   }
   template < TriviallySerializable T >
   T peek() {
      auto val = try_peek< T >();
      if ( !val )
         throw std::runtime_error( "BytesReader underflow" );
      return val.value();
   }
   template < TriviallySerializable T >
   std::optional< T > try_read() {
      auto val = try_peek< T >();
      if ( val ) {
         remBytes_ -= sizeof( T );
      }
      return val;
   }
   template < TriviallySerializable T >
   void transfer( T & result ) {
      auto res = try_read< T >();
      if ( !res )
         throw std::runtime_error( "BytesReader underflow" );
      result = res.value();
   }
   template < TriviallySerializable T >
   T transfer() {
      T val;
      transfer< T >( val );
      return val;
   }
   template < ContiguousDynamicallySized Coll >
   void transfer( Coll & coll ) {
      using Elem = std::ranges::range_value_t< Coll >;
      std::size_t writeSize = coll.size() * sizeof( Elem );
      if ( remBytes_ < writeSize ) {
         throw std::runtime_error( "BytesReader underflow" );
      }
      std::size_t currentIdx = bytes_.size() - remBytes_;
      std::memcpy(
          std::ranges::data( coll ), bytes_.data() + currentIdx, writeSize );
      remBytes_ -= writeSize;
   }

   std::size_t remainingBytes() { return remBytes_; }

 private:
   std::span< uint8_t > bytes_;
   std::size_t remBytes_;
};

} // namespace Serdes
