add_subdirectory(scheduler)
add_subdirectory(comparator)

add_library(sim-gui Gui.cpp)
target_include_directories(sim-gui SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/include/")
target_link_libraries(sim-gui PUBLIC GL glfw imgui implot)
