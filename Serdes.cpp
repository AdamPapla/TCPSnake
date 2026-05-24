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
   writer.write( narrow_size( snapshot.snakes ) );
   for ( auto & snake : snapshot.snakes ) {
      writer.write( snake.id );
      writer.write( narrow_size( snake.blocks ) );
      writer.writeBytes( snake.blocks.data(), snake.blocks.size() );
   }
   writer.write( narrow_size( snapshot.food ) );
   writer.writeBytes( snapshot.food.data(), snapshot.food.size() );
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
   auto numSnakes = reader.read< std::uint16_t >();
   snapshot.snakes.resize( numSnakes );
   for ( uint16_t i = 0; i < numSnakes; ++i ) {
      SnakeSnapshot snake;
      snake.id = reader.read< ClientId >();
      auto numBlocks = reader.read< std::uint16_t >();
      snake.blocks.resize( numBlocks );
      reader.readBytes< Coord >( snake.blocks );
      snapshot.snakes[ i ] = std::move( snake );
   }
   auto numFood = reader.read< std::uint16_t >();
   snapshot.food.resize( numFood );
   reader.readBytes< Coord >( snapshot.food );

   assert( reader.remainingBytes() == 0 && "Reader didn't fully read it's payload" );

   return snapshot;
}

} // namespace Serdes
