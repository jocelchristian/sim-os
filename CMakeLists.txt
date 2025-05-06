cmake_minimum_required(VERSION 3.30.5)
project(sim-os)

set(CXX_FLAGS "-std=c++2b -Wall -Wextra -Werror -Wshadow -Wconversion -Wpedantic -ggdb")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(scheduler scheduler.cpp)
