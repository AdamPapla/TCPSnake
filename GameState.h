#pragma once

#include <vector>

#include "Utility.h"

namespace GameState {

struct SnakeSnapshot {
   ClientId id;
   std::vector< Coord > blocks;
};

struct Snapshot {
   // Since the map is sparsely populated, prefer to send coordinates
   std::vector< SnakeSnapshot > snakes;
   std::vector< Coord > food;
};

} // namespace GameState
