# Minimum CMake version required
cmake_minimum_required(VERSION 3.15)

# Project name and language
project(SnakeGame LANGUAGES CXX)

# Set C++ standard and compiler flags
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic -Wshadow -Wconversion -g -freflection -std=c++26")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fsanitize=undefined -fsanitize=address")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

# Define library with game files
ADD_LIBRARY( SnakeLibModule
   SnakeGame.cpp Utility.cpp SnakeGameLogic.cpp SnakeClient.cpp
)
target_include_directories(SnakeLibModule PUBLIC ${CMAKE_SOURCE_DIR})

# Find and link raylib
target_link_libraries( SnakeLibModule -lraylib )

# Set the path to the raylib headers (assuming Raylib headers are in /usr/local/include)
include_directories(/usr/local/include)

ADD_EXECUTABLE( SnakeGame main.cpp )

target_link_libraries( SnakeGame SnakeLibModule )

find_package(GTest REQUIRED)

# Testing
set(TEST_FILES 
    test/SerdesRoundtripTest.cpp
    test/ClientIntegrationTest.cpp
)

# Add and link test executables
foreach(TEST_FILE ${TEST_FILES})
    get_filename_component(TEST_NAME ${TEST_FILE} NAME_WE)
    add_executable(${TEST_NAME} ${TEST_FILE})
    target_link_libraries(${TEST_NAME} PRIVATE SnakeLibModule GTest::GTest GTest::Main)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

enable_testing()

# Enable Clang-Tidy
find_program(CLANG_TIDY_EXE NAMES clang-tidy)
if(CLANG_TIDY_EXE)
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

# Custom clean target (optional, mirrors Makefile clean behavior)
add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_BINARY_DIR}
    COMMENT "Clean up build artifacts."
)

