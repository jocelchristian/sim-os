add_subdirectory("lang")
add_subdirectory("gui")

add_library(sim-util
    ${CMAKE_SOURCE_DIR}/src/Util.cpp
)
