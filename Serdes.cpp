#include "Serdes.h"
#include <limits>

namespace {

using namespace GameState;

template < typename T >
std::uint16_t
narrow_size( const std::vector< T > & vec ) {
   assert( vec.size() < std::numeric_limits< std::uint16_t >::max() );
   return static_cast< uint16_t >( vec.size() );
}
// We'll do two passes - one to determine length of bytes array to allocate and
// another to write to it. Keep them coupled to avoid a mismatch between fields
// counted and fields written
template < typename BytesOperator >
void
serialize( const Snapshot & snapshot, BytesOperator & writer ) {
   writer.transfer( narrow_size( snapshot.snakes ) );
   for ( auto & snake : snapshot.snakes ) {
      writer.transfer( snake.id );
      writer.transfer( narrow_size( snake.blocks ) );
      writer.transfer( snake.blocks );
   }
   writer.transfer( narrow_size( snapshot.food ) );
   writer.transfer( snapshot.food );
}

} // namespace
namespace Serdes {

using namespace GameState;
std::vector< uint8_t >
writeSnapshot( const Snapshot & snapshot ) {
   BytesCounter counter;
   serialize( snapshot, counter );
   std::vector< std::uint8_t > byteStream( counter.count );

   BytesWriter writer( byteStream );
   serialize( snapshot, writer );
   assert( writer.remainingBytes() == 0 &&
           "Writer didn't fully write it's payload" );
   return byteStream;
}

// TODO: Revisit this functionality now that read returns optional
// We now don't really need bounds checking here - we can just read directly from
// bytes since message is guaranteed to be well-formed at this point.
Snapshot
readSnapshot( BytesReader & reader ) {
   Snapshot snapshot;
   std::uint16_t numSnakes = UINT16_MAX;
   reader.transfer< std::uint16_t >( numSnakes );
   snapshot.snakes.resize( numSnakes );
   for ( uint16_t i = 0; i < numSnakes; ++i ) {
      SnakeSnapshot snake;
      reader.transfer< ClientId >( snake.id );
      auto numBlocks = reader.transfer< std::uint16_t >();
      snake.blocks.resize( numBlocks );
      reader.transfer( snake.blocks );
      snapshot.snakes[ i ] = std::move( snake );
   }
   auto numFood = reader.transfer< std::uint16_t >();
   snapshot.food.resize( numFood );
   reader.transfer( snapshot.food );

   assert( reader.remainingBytes() == 0 && "Reader didn't fully read it's payload" );

   return snapshot;
}

} // namespace Serdes
