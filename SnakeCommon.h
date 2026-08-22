#pragma once
#include <span>
#include <stop_token>

#include "BytesOperator.h"
#include "Messages.h"
#include "SnakeSerdes.h"
#include "TSQueue.h"

namespace SessionCommon {

template < typename T >
class Accumulator : public std::vector< T > {
 public:
   Accumulator() = default;
   Accumulator( size_t compactAfter ) : compactAfter_{ compactAfter } {}
   size_t readOffset() const { return readOffset_; }
   void setReadOffset( const size_t readOffset ) { readOffset_ = readOffset; }
   void maybeCompact() {
      if ( this->size() > compactAfter_ ) {
         // TODO: This may lead to wasted allocations once the buffer fills again
         this->erase( this->begin(), this->begin() + readOffset_ );
         readOffset_ = 0;
      }
   }
   std::span< T > readBuffer() {
      return { this->begin() + readOffset_, this->end() };
   }
   void updateOffset( const size_t remaining ) {
      readOffset_ = this->size() - remaining;
   }

 private:
   size_t compactAfter_{ 1024 };
   size_t readOffset_{ 0 };
};

template < typename Message >
static void
onReceive( Accumulator< std::uint8_t > & accumulator,
           TSQueue< Message > & msgQueue,
           const std::stop_token & stop ) {
   Serdes::BytesReader reader( accumulator.readBuffer() );
   while ( !stop.stop_requested() ) {
      if ( auto msg = SnakeSerdes::readNext< Message >( reader ); msg ) {
         assert( msg.has_value() );
         msgQueue.push( std::move( msg.value() ) );
      } else
         break;
   }
   accumulator.updateOffset( reader.remainingBytes() );
   accumulator.maybeCompact();
}

} // namespace SessionCommon
