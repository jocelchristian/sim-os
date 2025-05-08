cmake_minimum_required(VERSION 3.30.5)
project(sim-os)

set(CXX_FLAGS "-std=c++2b -Wall -Wextra -Werror -Wshadow -Wconversion -Wpedantic -ggdb")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory("${CMAKE_SOURCE_DIR}/src/")

add_executable(
    scheduler
    examples/scheduler/scheduler.cpp
)
target_include_directories(scheduler PRIVATE "${CMAKE_SOURCE_DIR}/src/")
target_link_libraries(scheduler PRIVATE sim-lang sim-simulations)
