#pragma once
#include <concepts>
#include <cstdint>
#include <cstring>
#include <optional>
#include <span>
#include <stdexcept>
#include <type_traits>

#include "GameState.h"

namespace Serdes {

template < typename T >
concept TriviallySerializable =
    std::is_trivially_copyable_v< T > && std::is_standard_layout_v< T >;

struct BytesCounter {
   std::size_t count{ 0 };

   template < TriviallySerializable T >
   void write( const T & ) {
      count += sizeof( T );
   }
   template < TriviallySerializable T >
   void writeBytes( const T *const, const std::size_t size ) {
      count += size * sizeof( T );
   }
};

class BytesWriter {
 public:
   BytesWriter( std::span< std::uint8_t > bytes )
       : bytes_{ bytes }, remBytes_{ bytes_.size() } {}

   template < TriviallySerializable T >
   void write( const T & val ) {
      if ( remBytes_ < sizeof( T ) ) {
         throw std::runtime_error( "BytesWriter overflow" );
      }
      std::size_t currentIdx = bytes_.size() - remBytes_;
      std::memcpy( bytes_.data() + currentIdx, &val, sizeof( T ) );
      remBytes_ -= sizeof( T );
   }
   template < TriviallySerializable T >
   void writeBytes( const T *const data, const std::size_t size ) {
      std::size_t currentIdx = bytes_.size() - remBytes_;
      std::size_t writeSize = size * sizeof( T );
      if ( remBytes_ < writeSize ) {
         throw std::runtime_error( "BytesWriter overflow" );
      }
      std::memcpy( bytes_.data() + currentIdx, data, writeSize );
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
      auto val = try_peek<T>();
      if ( !val ) throw std::runtime_error("BytesReader underflow");
      return val.value();
   }
   template< TriviallySerializable T >
   std::optional< T > try_read() {
      auto val = try_peek< T >();
      if ( val ) {
         remBytes_ -= sizeof( T );
      }
      return val;
   }
   template < TriviallySerializable T >
   T read() {
      auto res = try_read< T >();
      if ( !res ) throw std::runtime_error( "BytesReader underflow" );
      return res.value();
   }
   template < TriviallySerializable T >
   void readBytes( std::span< T > dest ) {
      std::size_t writeSize = dest.size_bytes();
      if ( remBytes_ < writeSize ) {
         throw std::runtime_error( "BytesReader underflow" );
      }
      std::size_t currentIdx = bytes_.size() - remBytes_;
      std::memcpy( dest.data(), bytes_.data() + currentIdx, writeSize );
      remBytes_ -= writeSize;
   }

   void advance( std::size_t len ) { remBytes_ -= len; }

   std::size_t remainingBytes() { return remBytes_; }

 private:
   std::span< uint8_t > bytes_;
   std::size_t remBytes_;
};

} // namespace Serdes
