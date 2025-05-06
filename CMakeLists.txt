cmake_minimum_required(VERSION 3.30.5)
project(sim-os)

set(CXX_FLAGS "-std=c++2b -Wall -Wextra -Werror -Wshadow -Wconversion -Wpedantic -ggdb")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Debug)

add_executable(scheduler
    src/scheduler.cpp
    src/Lexer.cpp
    src/Parser.cpp
    src/Interpreter.cpp
)
