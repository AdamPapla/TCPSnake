#pragma once

#include <vector>

#include "Utility.h"

namespace GameState {

struct SnakeSnapshot {
   ClientId id;
   std::vector< Coord > blocks;
   bool operator==( const SnakeSnapshot & other ) const {
      if ( id != other.id || blocks.size() != other.blocks.size() )
         return false;
      for ( size_t i = 0; i < blocks.size(); ++i ) {
         if ( blocks[ i ][ 0 ] != other.blocks[ i ][ 0 ] ||
              blocks[ i ][ 1 ] != other.blocks[ i ][ 1 ] )
            return false;
      }
      return true;
   }
};

struct Snapshot {
   // Since the map is sparsely populated, prefer to send coordinates
   std::vector< SnakeSnapshot > snakes;
   std::vector< Coord > food;
   bool operator==( const Snapshot & other ) const {
      if ( snakes.size() != other.snakes.size() || food.size() != other.food.size() )
         return false;
      for ( size_t i = 0; i < snakes.size(); ++i ) {
         if ( snakes[ i ] != other.snakes[ i ] )
            return false;
      }
      for ( size_t i = 0; i < food.size(); ++i ) {
         if ( food[ i ][ 0 ] != other.food[ i ][ 0 ] ||
              food[ i ][ 1 ] != other.food[ i ][ 1 ] )
            return false;
      }
      return true;
   }
};

} // namespace GameState
