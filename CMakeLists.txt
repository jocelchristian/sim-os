cmake_minimum_required(VERSION 3.30.5)
project(sim-os)

include(FetchContent)

set(CXX_FLAGS "-std=c++2b -Wall -Wextra -Werror -Wshadow -Wconversion -Wpedantic -ggdb")
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_subdirectory("${CMAKE_SOURCE_DIR}/src/")

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG latest
)
FetchContent_MakeAvailable(glfw)
include_directories(${GLFW_INCLUDE_DIRS})

FetchContent_Declare(imgui_external
	URL https://github.com/ocornut/imgui/archive/refs/tags/v1.90.8.tar.gz
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(imgui_external)

add_library(imgui
	${imgui_external_SOURCE_DIR}/imgui.cpp
	${imgui_external_SOURCE_DIR}/imgui_draw.cpp
	${imgui_external_SOURCE_DIR}/imgui_tables.cpp
	${imgui_external_SOURCE_DIR}/imgui_widgets.cpp
	${imgui_external_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
	${imgui_external_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)
target_include_directories(imgui PUBLIC ${imgui_external_SOURCE_DIR})
target_link_libraries(imgui PUBLIC glfw)

FetchContent_Declare(implot_external
        URL https://github.com/epezent/implot/archive/refs/tags/v0.16.tar.gz
	EXCLUDE_FROM_ALL
)
FetchContent_MakeAvailable(implot_external)

add_library(implot
	${implot_external_SOURCE_DIR}/implot.cpp
	${implot_external_SOURCE_DIR}/implot_items.cpp
)
target_include_directories(implot PUBLIC ${implot_external_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui glfw)

add_executable(
    scheduler
    src/gui/scheduler/scheduler_main.cpp
)
target_include_directories(scheduler PRIVATE "${CMAKE_SOURCE_DIR}/src/" "${CMAKE_SOURCE_DIR}/include")
target_link_libraries(scheduler PRIVATE sim-lang GL glfw imgui implot)
target_compile_definitions(scheduler PRIVATE $<$<CONFIG:Debug>:DEBUG>)

