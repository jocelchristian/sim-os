add_executable(
    scheduler
    ${CMAKE_SOURCE_DIR}/src/gui/scheduler/main.cpp
    ${CMAKE_SOURCE_DIR}/src/gui/scheduler/Application.cpp
)
set_target_properties(scheduler PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(scheduler PRIVATE "${CMAKE_SOURCE_DIR}/src/")
target_link_libraries(scheduler PRIVATE sim-lang sim-gui sim-util)
target_compile_definitions(scheduler PRIVATE $<$<CONFIG:Debug>:DEBUG>)
target_compile_features(scheduler PRIVATE cxx_std_23)
target_compile_options(scheduler
    PRIVATE
        -Wall
        -Wextra
        -Werror
        -Wconversion
        -Wno-maybe-uninitialized
        $<$<CONFIG:Debug>: -ggdb -O0 -fno-omit-frame-pointer>
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_SANITIZERS}>>: -fsanitize=undefined,address,leak>
        $<$<CONFIG:Release>: -O3>)
target_link_options(scheduler
    PRIVATE
        $<$<CONFIG:Debug>:>
        $<$<AND:$<CONFIG:Debug>,$<BOOL:${ENABLE_SANITIZERS}>>: -fsanitize=undefined,address,leak>
)
