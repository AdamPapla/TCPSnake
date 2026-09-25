#pragma once

#include <cstdint>
#include <format>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#define GRID_SIZE 100
using ClientId = std::uint16_t;
using Coord = std::array< std::uint32_t, 2 >;

enum class Move { left, right, up, down, stay };

std::string getDir( Move move );

// TODO: Move to spdlog or some level-based logging
static std::mutex logMutex;
template < typename... Args >
void
LOG( std::format_string< Args... > str, Args &&...args ) {
   auto guard{ std::lock_guard( logMutex ) };
   std::cout << "[" << std::this_thread::get_id() << "]"
             << std::format( str, std::forward< Args >( args )... ) << std::endl;
}
