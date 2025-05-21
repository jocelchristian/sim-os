cmake_minimum_required(VERSION 3.30.5)
project(sim-os)

include(FetchContent)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STARNDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(ENABLE_SANITIZERS "Enable address and leak sanitizers" OFF)

add_subdirectory("${CMAKE_SOURCE_DIR}/src/")

FetchContent_Declare(
  glfw
  GIT_REPOSITORY https://github.com/glfw/glfw.git
  GIT_TAG latest
)
FetchContent_MakeAvailable(glfw)

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
target_include_directories(imgui SYSTEM PUBLIC ${imgui_external_SOURCE_DIR})
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
target_include_directories(implot SYSTEM PUBLIC ${implot_external_SOURCE_DIR})
target_link_libraries(implot PUBLIC imgui glfw)

