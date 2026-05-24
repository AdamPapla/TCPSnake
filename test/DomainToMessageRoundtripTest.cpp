// SerdesTest.cpp

#include <gtest/gtest.h>

#include "../Messages.h"
#include "../Serdes.h"

using namespace GameState;
using namespace Serdes;

namespace {

bool
coordEqual( const Coord & a, const Coord & b ) {
   return a[ 0 ] == b[ 0 ] && a[ 1 ] == b[ 1 ];
}

bool
snakeEqual( const SnakeSnapshot & a, const SnakeSnapshot & b ) {
   if ( a.id != b.id )
      return false;
   if ( a.blocks.size() != b.blocks.size() )
      return false;

   for ( size_t i = 0; i < a.blocks.size(); ++i ) {
      if ( !coordEqual( a.blocks[ i ], b.blocks[ i ] ) )
         return false;
   }
   return true;
}

bool
snapshotEqual( const Snapshot & a, const Snapshot & b ) {
   if ( a.snakes.size() != b.snakes.size() )
      return false;
   if ( a.food.size() != b.food.size() )
      return false;

   for ( size_t i = 0; i < a.snakes.size(); ++i ) {
      if ( !snakeEqual( a.snakes[ i ], b.snakes[ i ] ) )
         return false;
   }

   for ( size_t i = 0; i < a.food.size(); ++i ) {
      if ( !coordEqual( a.food[ i ], b.food[ i ] ) )
         return false;
   }

   return true;
}

Coord
makeCoord( std::uint32_t x, std::uint32_t y ) {
   Coord c{};
   c[ 0 ] = x;
   c[ 1 ] = y;
   return c;
}

SnakeSnapshot
makeSnake( ClientId id, std::initializer_list< Coord > blocks ) {
   SnakeSnapshot s{};
   s.id = id;
   s.blocks = blocks;
   return s;
}

} // namespace

TEST( SerdesRoundTrip, EmptySnapshot ) {
   Snapshot original{};

   auto bytes = writeSnapshot( original );
   BytesReader reader( bytes );
   Snapshot decoded = readSnapshot( reader );

   EXPECT_TRUE( snapshotEqual( original, decoded ) );
}

TEST( SerdesRoundTrip, SingleSnakeNoFood ) {
   Snapshot original;

   original.snakes.push_back(
       makeSnake( 1, { makeCoord( 1, 1 ), makeCoord( 1, 2 ), makeCoord( 1, 3 ) } ) );

   auto bytes = writeSnapshot( original );
   BytesReader reader( bytes );
   Snapshot decoded = readSnapshot( reader );

   EXPECT_TRUE( snapshotEqual( original, decoded ) );
}

TEST( SerdesRoundTrip, MultipleSnakesAndFood ) {
   Snapshot original;

   original.snakes.push_back(
       makeSnake( 1, { makeCoord( 0, 0 ), makeCoord( 0, 1 ), makeCoord( 0, 2 ) } ) );

   original.snakes.push_back(
       makeSnake( 2, { makeCoord( 5, 5 ), makeCoord( 5, 6 ) } ) );

   original.food = { makeCoord( 10, 10 ), makeCoord( 3, 7 ), makeCoord( 8, 2 ) };

   auto bytes = writeSnapshot( original );
   BytesReader reader( bytes );
   Snapshot decoded = readSnapshot( reader );

   EXPECT_TRUE( snapshotEqual( original, decoded ) );
}

TEST( SerdesRoundTrip, LargeSnake ) {
   Snapshot original;

   SnakeSnapshot snake{};
   snake.id = 42;

   for ( uint32_t i = 0; i < 100; ++i ) {
      snake.blocks.push_back( makeCoord( i, i + 1 ) );
   }

   original.snakes.push_back( snake );

   auto bytes = writeSnapshot( original );
   BytesReader reader( bytes );
   Snapshot decoded = readSnapshot( reader );

   EXPECT_TRUE( snapshotEqual( original, decoded ) );
}
